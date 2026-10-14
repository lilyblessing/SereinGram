// Generated from proto/serein/config/v1/schedule.proto by tools/serein/codegen; do not edit.
#pragma once

#include "serein/schema/codec.h"

namespace Serein::Schedule {

struct DailySchedule {
	QString peer;
	int minuteOfDay = 0;
	QString text;
	QString lastSentDate;
	bool serverScheduled = false;

	friend bool operator==(const DailySchedule &, const DailySchedule &) = default;
};

[[nodiscard]] bool Read(
	const QJsonValue &json,
	DailySchedule &result,
	Codec::Error &error,
	const QString &path);
[[nodiscard]] QJsonValue Write(const DailySchedule &value);
[[nodiscard]] bool Validate(
	const DailySchedule &value,
	Codec::Error &error,
	const QString &path);

struct DailySchedules {
	std::vector<DailySchedule> entries;

	friend bool operator==(const DailySchedules &, const DailySchedules &) = default;
};

[[nodiscard]] bool Read(
	const QJsonValue &json,
	DailySchedules &result,
	Codec::Error &error,
	const QString &path);
[[nodiscard]] QJsonValue Write(const DailySchedules &value);
[[nodiscard]] bool Validate(
	const DailySchedules &value,
	Codec::Error &error,
	const QString &path);
[[nodiscard]] std::optional<DailySchedules> ParseDailySchedules(
	const QByteArray &raw,
	Codec::Error *error = nullptr);
[[nodiscard]] QByteArray SerializeDailySchedules(const DailySchedules &value);

} // namespace Serein::Schedule
