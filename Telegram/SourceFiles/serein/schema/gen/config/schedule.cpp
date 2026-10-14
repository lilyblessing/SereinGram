// Generated from proto/serein/config/v1/schedule.proto by tools/serein/codegen; do not edit.
#include "serein/schema/gen/config/schedule.h"

namespace Serein::Schedule {

bool Read(
		const QJsonValue &json,
		DailySchedule &result,
		Codec::Error &error,
		const QString &path) {
	if (!json.isObject()) {
		return Codec::FailExpected(error, path, "an object");
	}
	const auto object = json.toObject();
	if (!Codec::KnownKeys(object, {
			QLatin1StringView("peer"),
			QLatin1StringView("minuteOfDay"),
			QLatin1StringView("text"),
			QLatin1StringView("lastSentDate"),
			QLatin1StringView("serverScheduled"),
		}, error, path)) {
		return false;
	} else if (!Codec::RequiredKeys(object, {
			QLatin1StringView("peer"),
			QLatin1StringView("minuteOfDay"),
			QLatin1StringView("text"),
			QLatin1StringView("lastSentDate"),
			QLatin1StringView("serverScheduled"),
		}, error, path)) {
		return false;
	}
	result = DailySchedule();
	return true
		&& Codec::ReadField(object, QLatin1StringView("peer"), result.peer, error, path)
		&& Codec::ReadField(object, QLatin1StringView("minuteOfDay"), result.minuteOfDay, error, path)
		&& Codec::ReadField(object, QLatin1StringView("text"), result.text, error, path)
		&& Codec::ReadField(object, QLatin1StringView("lastSentDate"), result.lastSentDate, error, path)
		&& Codec::ReadField(object, QLatin1StringView("serverScheduled"), result.serverScheduled, error, path);
}

QJsonValue Write(const DailySchedule &value) {
	auto object = QJsonObject();
	Codec::WriteField(object, QLatin1StringView("peer"), value.peer);
	Codec::WriteField(object, QLatin1StringView("minuteOfDay"), value.minuteOfDay);
	Codec::WriteField(object, QLatin1StringView("text"), value.text);
	Codec::WriteField(object, QLatin1StringView("lastSentDate"), value.lastSentDate);
	Codec::WriteField(object, QLatin1StringView("serverScheduled"), value.serverScheduled);
	return object;
}

bool Validate(
		const DailySchedule &value,
		Codec::Error &error,
		const QString &path) {
	if (!(Codec::Matches(value.peer, QString::fromUtf8("^[1-9][0-9]{0,19}$")))) {
		return Codec::Fail(error, Codec::Child(path, QLatin1StringView("peer")), QString::fromLatin1("violates the schema rules"));
	}
	if (!(value.minuteOfDay <= 1439 && value.minuteOfDay >= 0)) {
		return Codec::Fail(error, Codec::Child(path, QLatin1StringView("minuteOfDay")), QString::fromLatin1("violates the schema rules"));
	}
	if (!(value.text.toUcs4().size() <= 4096)) {
		return Codec::Fail(error, Codec::Child(path, QLatin1StringView("text")), QString::fromLatin1("violates the schema rules"));
	}
	if (!(value.lastSentDate.toUcs4().size() <= 10)) {
		return Codec::Fail(error, Codec::Child(path, QLatin1StringView("lastSentDate")), QString::fromLatin1("violates the schema rules"));
	}
	return true;
}

bool Read(
		const QJsonValue &json,
		DailySchedules &result,
		Codec::Error &error,
		const QString &path) {
	if (!json.isObject()) {
		return Codec::FailExpected(error, path, "an object");
	}
	const auto object = json.toObject();
	if (!Codec::KnownKeys(object, {
			QLatin1StringView("entries"),
		}, error, path)) {
		return false;
	} else if (!Codec::RequiredKeys(object, {
			QLatin1StringView("entries"),
		}, error, path)) {
		return false;
	}
	result = DailySchedules();
	return true
		&& Codec::ReadField(object, QLatin1StringView("entries"), result.entries, error, path);
}

QJsonValue Write(const DailySchedules &value) {
	auto object = QJsonObject();
	Codec::WriteField(object, QLatin1StringView("entries"), value.entries);
	return object;
}

bool Validate(
		const DailySchedules &value,
		Codec::Error &error,
		const QString &path) {
	if (!(qsizetype(value.entries.size()) <= 200)) {
		return Codec::Fail(error, Codec::Child(path, QLatin1StringView("entries")), QString::fromLatin1("violates the schema rules"));
	}
	for (auto i = qsizetype(); i != qsizetype(value.entries.size()); ++i) {
		const auto &item = value.entries[i];
		if (!Validate(item, error, Codec::Item(Codec::Child(path, QLatin1StringView("entries")), i))) {
			return false;
		}
	}
	return true;
}

std::optional<DailySchedules> ParseDailySchedules(
		const QByteArray &raw,
		Codec::Error *error) {
	auto ignored = Codec::Error();
	auto &out = error ? *error : ignored;
	auto object = Codec::ParseObject(raw, out);
	if (!object) {
		return std::nullopt;
	} else if (object->value(QLatin1StringView("version")) != QJsonValue(1)) {
		Codec::Fail(out, QString::fromLatin1("version"), QString::fromLatin1("unsupported version"));
		return std::nullopt;
	}
	object->remove(QLatin1StringView("version"));
	auto result = DailySchedules();
	if (!Read(*object, result, out, QString()) || !Validate(result, out, QString())) {
		return std::nullopt;
	}
	return result;
}

QByteArray SerializeDailySchedules(const DailySchedules &value) {
	auto object = Write(value).toObject();
	object.insert(QLatin1StringView("version"), 1);
	return Codec::Serialize(object);
}

} // namespace Serein::Schedule
