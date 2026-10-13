#include "serein/filters/model.h"

#include "serein/core/patterns.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QUuid>

#include <algorithm>
#include <map>
#include <mutex>

namespace Serein::Filters {
namespace {

constexpr auto kMaxText = 16384;
constexpr auto kMaxMatches = 256;
constexpr auto kMaxWorkMs = 20;
constexpr auto kMaxConfigBytes = 128 * 1024;

bool ValidId(const QString &id) {
	auto ok = false;
	const auto number = id.toULongLong(&ok);
	return ok && number && QString::number(number) == id;
}

bool ValidText(const QString &text, int limit, bool allowEmpty) {
	return (allowEmpty || !text.isEmpty()) && text.size() <= limit
		&& !text.contains(QChar(0))
		&& QString::fromUtf8(text.toUtf8()) == text;
}

struct Edit {
	int start = 0;
	int end = 0;
	QString replacement;
};

bool ApplyEdits(TextWithEntities &text, const std::vector<Edit> &edits) {
	const auto original = text;
	auto offsets = std::vector<int>(original.text.size() + 1, -1);
	auto result = TextWithEntities();
	auto cursor = 0;
	for (const auto &edit : edits) {
		if (edit.start < cursor || edit.end <= edit.start
			|| edit.end > original.text.size()) {
			return false;
		}
		for (; cursor < edit.start; ++cursor) {
			offsets[cursor] = result.text.size();
			result.text += original.text[cursor];
		}
		offsets[edit.start] = result.text.size();
		result.text += edit.replacement;
		cursor = edit.end;
		offsets[cursor] = result.text.size();
	}
	for (; cursor < original.text.size(); ++cursor) {
		offsets[cursor] = result.text.size();
		result.text += original.text[cursor];
	}
	offsets.back() = result.text.size();
	if (result.text.size() > kMaxText) {
		return false;
	}
	for (const auto &entity : original.entities) {
		if (!entity.validForText(original.text.size())) {
			return false;
		}
		const auto start = entity.offset();
		const auto end = start + entity.length();
		const auto overlaps = std::any_of(edits.begin(), edits.end(), [&](const Edit &edit) {
			return edit.start < end && edit.end > start;
		});
		if (overlaps || offsets[start] < 0 || offsets[end] < offsets[start]) {
			continue;
		}
		auto adjusted = entity;
		adjusted.shiftLeft(start - offsets[start]);
		adjusted.shrinkFromRight(entity.length()
			- (offsets[end] - offsets[start]));
		result.entities.push_back(std::move(adjusted));
	}
	text = std::move(result);
	return true;
}

std::vector<Edit> ZalgoEdits(const QString &text) {
	auto edits = std::vector<Edit>();
	auto marks = 0;
	for (auto i = 0; i < text.size();) {
		const auto start = i;
		auto scalar = uint(text[i++].unicode());
		if (QChar::isHighSurrogate(scalar) && i < text.size()) {
			scalar = QChar::surrogateToUcs4(QChar(scalar), text[i++]);
		}
		const auto category = QChar::category(scalar);
		const auto combining = category == QChar::Mark_NonSpacing
			|| category == QChar::Mark_SpacingCombining
			|| category == QChar::Mark_Enclosing;
		marks = combining ? marks + 1 : 0;
		if (marks > 3 && scalar != 0xFE0E && scalar != 0xFE0F
			&& scalar != 0x20E3
			&& !(scalar >= 0xE0100 && scalar <= 0xE01EF)) {
			edits.push_back({ start, i, QString() });
		}
	}
	return edits;
}

[[nodiscard]] QByteArray Upgraded(const QByteArray &raw, int target) {
	auto root = QJsonDocument::fromJson(raw).object();
	const auto version = root.value(u"version"_q).toInt();
	if (version <= 0 || version >= target) {
		return raw;
	}
	if (version == 1) {
		if (const auto value = root.value(u"rules"_q); value.isArray()) {
			auto rules = QJsonArray();
			for (const auto &entry : value.toArray()) {
				auto rule = entry.toObject();
				if (entry.isObject() && !rule.contains(u"peers"_q)) {
					rule.insert(u"peers"_q, QJsonArray());
				}
				rules.push_back(entry.isObject() ? QJsonValue(rule) : entry);
			}
			root.insert(u"rules"_q, rules);
		}
	}
	if (target >= 3) {
		const auto addMissing = [&](const QString &key, const QJsonValue &value) {
			if (!root.contains(key)) {
				root.insert(key, value);
			}
		};
		addMissing(u"hideBlockedAction"_q, u"hide"_q);
		addMissing(u"hiddenAuthorsAction"_q, u"hide"_q);
		addMissing(u"hideBlockedReplacement"_q, QString());
		addMissing(u"hiddenAuthorsReplacement"_q, QString());
	}
	root.insert(u"version"_q, target);
	return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace

std::optional<FilterRules> ReadRules(const QByteArray &raw) {
	return raw.isEmpty() ? FilterRules() : ParseFilterRules(Upgraded(raw, 3));
}

std::optional<FilterRules> CachedRules(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return FilterRules();
	}
	constexpr auto kMaxCachedRules = 4;
	static std::mutex mutex;
	static std::map<QByteArray, std::optional<FilterRules>> cache;
	{
		const auto lock = std::lock_guard(mutex);
		if (const auto i = cache.find(raw); i != cache.end()) {
			return i->second;
		}
	}
	auto parsed = ParseFilterRules(Upgraded(raw, 3));
	const auto lock = std::lock_guard(mutex);
	if (cache.find(raw) == cache.end()) {
		while (cache.size() >= kMaxCachedRules) {
			cache.erase(cache.begin());
		}
		cache.emplace(raw, parsed);
	}
	return parsed;
}

std::optional<std::vector<FilterRule>> ReadRuleList(const QByteArray &raw) {
	if (raw.size() > kMaxConfigBytes) {
		return std::nullopt;
	}
	const auto list = ParseFilterRuleList(Upgraded(raw, 2));
	if (!list) {
		return std::nullopt;
	}
	auto check = FilterRules();
	check.rules = list->rules;
	return ValidFilterRules(check)
		? std::make_optional(list->rules)
		: std::nullopt;
}

QByteArray WriteRuleList(std::vector<FilterRule> rules) {
	return SerializeFilterRuleList({ .rules = std::move(rules) });
}

bool ValidFilterRules(const FilterRules &value) {
	if (SerializeFilterRules(value).size() > kMaxConfigBytes
		|| !std::ranges::all_of(value.hiddenAuthors, ValidId)
		|| !std::ranges::all_of(value.excludedPeers, ValidId)) {
		return false;
	}
	auto seen = QSet<QString>();
	for (const auto &rule : value.rules) {
		if (seen.contains(rule.id)
			|| QUuid(rule.id).isNull()
			|| !ValidText(rule.title, 128, false)
			|| !ValidText(rule.pattern, 2048, false)
			|| !ValidText(rule.replacement, 4096, true)
			|| (rule.reversed && rule.action != u"hide"_q
				&& rule.action != u"remove"_q)
			|| !SafePattern(rule.pattern, rule.caseInsensitive).isValid()) {
			return false;
		}
		seen.insert(rule.id);
	}
	return true;
}

bool Validate(const QByteArray &raw) {
	return CachedRules(raw).has_value();
}

bool ValidRuleList(const QByteArray &raw) {
	return raw.isEmpty() || ReadRuleList(raw).has_value();
}

Result Apply(
		const QByteArray &raw,
		const TextWithEntities &source,
		const QString &author,
		const QString &peer,
		bool blocked,
		bool outgoing,
		const QString &searchable,
		const std::vector<FilterRule> &shared,
		const QString &topic) {
	auto result = Result{ .text = source };
	if (raw.isEmpty()) {
		return result;
	}
	const auto config = CachedRules(raw);
	if (!config) {
		result.error = u"invalid filter configuration"_q;
		return result;
	}
	const auto contains = [](const std::vector<QString> &list, const QString &id) {
		return std::find(list.begin(), list.end(), id) != list.end();
	};
	if (!config->enabled
		|| contains(config->excludedPeers, peer)
		|| (outgoing && !config->filterOutgoing)) {
		return result;
	}
	if (blocked && config->hideBlocked) {
		const auto &action = config->hideBlockedAction;
		result.hidden = true;
		if (action == u"remove"_q) {
			result.removed = true;
		} else if (action == u"mask"_q) {
			result.substituted = true;
			result.substitution = TextWithEntities{ u"•••"_q };
		} else if (action == u"replace"_q) {
			result.substituted = true;
			result.substitution = TextWithEntities{
				config->hideBlockedReplacement };
		}
		return result;
	}
	if (contains(config->hiddenAuthors, author)) {
		const auto &action = config->hiddenAuthorsAction;
		result.hidden = true;
		if (action == u"remove"_q) {
			result.removed = true;
		} else if (action == u"mask"_q) {
			result.substituted = true;
			result.substitution = TextWithEntities{ u"•••"_q };
		} else if (action == u"replace"_q) {
			result.substituted = true;
			result.substitution = TextWithEntities{
				config->hiddenAuthorsReplacement };
		}
		return result;
	}
	if (source.text.size() > kMaxText
		|| (!searchable.isNull() && searchable.size() > kMaxText)) {
		result.error = u"filter text length limit"_q;
		return result;
	}
	auto timer = QElapsedTimer();
	timer.start();
	if (config->stripZalgo
		&& !ApplyEdits(result.text, ZalgoEdits(result.text.text))) {
		result.error = u"filter Zalgo edit failed"_q;
		return { .text = source, .error = result.error };
	}
	auto rules = std::vector<const FilterRule*>();
	for (const auto list : { &config->rules, &shared }) {
		for (const auto &rule : *list) {
			rules.push_back(&rule);
		}
	}
	for (const auto pointer : rules) {
		const auto &rule = *pointer;
		const auto inScope = rule.peers.empty()
			|| contains(rule.peers, peer)
			|| (!topic.isEmpty() && contains(rule.peers, topic));
		if (!rule.enabled || !inScope) {
			continue;
		}
		const auto expression = CachedSafePattern(
			rule.pattern,
			rule.caseInsensitive);
		const auto &action = rule.action;
		const auto wholeMessage = action == u"hide"_q
			|| action == u"remove"_q;
		const auto text = wholeMessage && !searchable.isNull()
			? searchable : result.text.text;
		auto edits = std::vector<Edit>();
		auto matched = false;
		for (auto offset = 0; offset <= text.size();) {
			if (timer.elapsed() >= kMaxWorkMs) {
				result.error = u"filter runtime limit"_q;
				break;
			}
			const auto match = expression.match(text, offset);
			if (!match.isValid()) {
				result.error = u"filter regex work limit"_q;
				break;
			}
			if (!match.hasMatch()) {
				break;
			}
			if (++result.matches > kMaxMatches) {
				result.error = u"filter match count limit"_q;
				break;
			}
			matched = true;
			if (wholeMessage) {
				break;
			}
			const auto start = int(match.capturedStart());
			const auto end = int(match.capturedEnd());
			if (end <= start) {
				offset = end + 1;
				continue;
			}
			edits.push_back({ start, end,
				action == u"mask"_q ? u"•••"_q : rule.replacement });
			offset = end;
		}
		if (!result.error.isEmpty()) {
			break;
		}
		if (wholeMessage) {
			if (matched != rule.reversed) {
				result.hidden = true;
				result.removed = (action == u"remove"_q);
				return result;
			}
		} else if (!ApplyEdits(result.text, edits)) {
			result.error = u"filter replacement limit"_q;
			break;
		}
	}
	if (!result.error.isEmpty()) {
		return { .text = source, .error = result.error,
			.matches = result.matches };
	}
	return result;
}

} // namespace Serein::Filters
