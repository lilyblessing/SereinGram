#pragma once

#include "serein/schema/gen/config/schedule.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace Serein::Schedule {

[[nodiscard]] std::optional<DailySchedules> ReadConfig(const QByteArray &raw);

[[nodiscard]] const DailySchedule *FindEntry(
	const DailySchedules &config,
	quint64 peer);

[[nodiscard]] std::optional<DailySchedules> WithEntry(
	const DailySchedules &config,
	quint64 peer,
	int minuteOfDay,
	const QString &text);

[[nodiscard]] std::optional<DailySchedules> WithoutEntry(
	const DailySchedules &config,
	quint64 peer);

[[nodiscard]] QByteArray SerializeConfig(const DailySchedules &config);

[[nodiscard]] std::optional<DailySchedules> MarkSent(
	const DailySchedules &config,
	quint64 peer,
	const QString &date);

[[nodiscard]] std::optional<DailySchedules> SetServerScheduled(
	const DailySchedules &config,
	quint64 peer,
	bool value);

[[nodiscard]] QString FormatMinute(int minuteOfDay);

} // namespace Serein::Schedule

namespace Serein::ServiceSettings {

[[nodiscard]] bool ValidDailySchedulesBytes(const QByteArray &value);

} // namespace Serein::ServiceSettings
