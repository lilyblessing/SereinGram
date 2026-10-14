#include "serein/schedule/model.h"

#include "serein/schema/gen/settings/services.h"
#include "base/basic_types.h"

#include <algorithm>
#include <vector>

#include <QtCore/QTime>

namespace Serein::Schedule {
namespace {

constexpr auto kMaxEntries = 200;

[[nodiscard]] bool SamePeer(const DailySchedule &entry, quint64 peer) {
	return entry.peer == QString::number(peer);
}

} // namespace

std::optional<DailySchedules> ReadConfig(const QByteArray &raw) {
	return raw.isEmpty()
		? std::make_optional(DailySchedules())
		: ParseDailySchedules(raw);
}

const DailySchedule *FindEntry(
		const DailySchedules &config,
		quint64 peer) {
	const auto i = std::ranges::find_if(config.entries, [&](
			const DailySchedule &entry) {
		return SamePeer(entry, peer);
	});
	return (i != config.entries.end()) ? &*i : nullptr;
}

std::optional<DailySchedules> WithEntry(
		const DailySchedules &config,
		quint64 peer,
		int minuteOfDay,
		const QString &text) {
	if (!peer || minuteOfDay < 0 || minuteOfDay > 1439) {
		return std::nullopt;
	}
	auto next = config;
	auto found = false;
	for (auto &entry : next.entries) {
		if (SamePeer(entry, peer)) {
			entry.minuteOfDay = minuteOfDay;
			entry.text = text;
			found = true;
		}
	}
	if (!found) {
		if (int(next.entries.size()) >= kMaxEntries) {
			return std::nullopt;
		}
		next.entries.push_back(DailySchedule{
			.peer = QString::number(peer),
			.minuteOfDay = minuteOfDay,
			.text = text,
		});
	}
	const auto raw = SerializeConfig(next);
	return (raw.isEmpty() || ParseDailySchedules(raw))
		? std::make_optional(std::move(next))
		: std::nullopt;
}

std::optional<DailySchedules> WithoutEntry(
		const DailySchedules &config,
		quint64 peer) {
	auto next = config;
	const auto removed = std::erase_if(next.entries, [&](
			const DailySchedule &entry) {
		return SamePeer(entry, peer);
	});
	if (!removed) {
		return std::make_optional(std::move(next));
	}
	const auto raw = SerializeConfig(next);
	return (raw.isEmpty() || ParseDailySchedules(raw))
		? std::make_optional(std::move(next))
		: std::nullopt;
}

QByteArray SerializeConfig(const DailySchedules &config) {
	return config.entries.empty()
		? QByteArray()
		: SerializeDailySchedules(config);
}

std::optional<DailySchedules> MarkSent(
		const DailySchedules &config,
		quint64 peer,
		const QString &date) {
	auto next = config;
	auto found = false;
	for (auto &entry : next.entries) {
		if (SamePeer(entry, peer)) {
			entry.lastSentDate = date;
			found = true;
		}
	}
	if (!found) {
		return std::nullopt;
	}
	const auto raw = SerializeConfig(next);
	return (raw.isEmpty() || ParseDailySchedules(raw))
		? std::make_optional(std::move(next))
		: std::nullopt;
}

std::optional<DailySchedules> SetServerScheduled(
		const DailySchedules &config,
		quint64 peer,
		bool value) {
	auto next = config;
	auto found = false;
	for (auto &entry : next.entries) {
		if (SamePeer(entry, peer)) {
			entry.serverScheduled = value;
			found = true;
		}
	}
	if (!found) {
		return std::nullopt;
	}
	const auto raw = SerializeConfig(next);
	return (raw.isEmpty() || ParseDailySchedules(raw))
		? std::make_optional(std::move(next))
		: std::nullopt;
}

QString FormatMinute(int minuteOfDay) {
	return QTime(minuteOfDay / 60, minuteOfDay % 60).toString(u"H:mm"_q);
}

} // namespace Serein::Schedule

namespace Serein::ServiceSettings {

bool ValidDailySchedulesBytes(const QByteArray &value) {
	return value.isEmpty()
		|| Serein::Schedule::ParseDailySchedules(value).has_value();
}

} // namespace Serein::ServiceSettings
