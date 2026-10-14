#include "serein/schedule/scheduler.h"

#include "serein/core/options.h"
#include "serein/schedule/model.h"
#include "serein/schema/gen/config/schedule.h"
#include "serein/schema/gen/settings/services.h"
#include "api/api_common.h"
#include "apiwrap.h"
#include "base/basic_types.h"
#include "core/application.h"
#include "data/data_chat_participant_status.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "history/history.h"
#include "main/main_session.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimer>

namespace Serein::Schedule {
namespace {

constexpr auto kTickInterval = 30 * 1000; // 30 seconds.
constexpr auto kDaySeconds = 24 * 60 * 60; // Telegram daily repeat period.
constexpr auto kCatchUpMinutes = 2 * 60; // Grace window after the set time.

[[nodiscard]] PeerData *TargetPeer(Main::Session *session, const QString &id) {
	auto ok = false;
	const auto serialized = id.toULongLong(&ok);
	return (ok && serialized)
		? session->data().peerLoaded(DeserializePeerId(serialized))
		: nullptr;
}

[[nodiscard]] TimeId NextOccurrence(int minuteOfDay) {
	const auto now = QDateTime::currentDateTime();
	auto target = QDateTime(
		now.date(),
		QTime(minuteOfDay / 60, minuteOfDay % 60));
	// WHY: the server rejects a schedule closer than ten seconds, so a target
	// that is already inside that window rolls over to tomorrow instead.
	if (target <= now.addSecs(10)) {
		target = target.addDays(1);
	}
	return TimeId(target.toSecsSinceEpoch());
}

[[nodiscard]] QString Today() {
	return QDateTime::currentDateTime().date().toString(u"yyyy-MM-dd"_q);
}

void Send(
		gsl::not_null<Main::Session*> session,
		gsl::not_null<PeerData*> peer,
		const DailySchedule &entry,
		TimeId scheduled) {
	auto action = Api::SendAction(session->data().history(peer));
	action.clearDraft = false;
	if (scheduled) {
		action.options.scheduled = scheduled;
		action.options.scheduleRepeatPeriod = kDaySeconds;
	}
	auto message = Api::MessageToSend(std::move(action));
	message.textWithTags = { entry.text };
	session->api().sendMessage(std::move(message));
}

// WHY: LEG B runs only while the app is alive and only for chats that are
// not server-scheduled, so a chat is never sent twice.
class DailyScheduler final : public QObject {
public:
	DailyScheduler(QObject *parent, gsl::not_null<Main::Session*> session)
	: QObject(parent)
	, _session(session) {
		_timer.setInterval(kTickInterval);
		connect(&_timer, &QTimer::timeout, this, [=] { check(); });
		_timer.start();
	}

private:
	void check() {
		const auto config = ReadConfig(
			ForAccount(_session).Get(ServiceSettings::kDailySchedules))
			.value_or(DailySchedules());
		if (config.entries.empty()) {
			return;
		}
		if (_session->premium()) {
			// WHY: Premium means LEG A owns every chat, so never fire the
			// local timer; ScheduleServerSide is a no-op once a chat is
			// already scheduled, so ticks cannot stack a second repeat.
			for (const auto &entry : config.entries) {
				if (const auto peer = TargetPeer(_session, entry.peer)) {
					ScheduleServerSide(_session, peer);
				}
			}
			return;
		}
		const auto now = QDateTime::currentDateTime();
		const auto today = now.date().toString(u"yyyy-MM-dd"_q);
		const auto minute = now.time().hour() * 60 + now.time().minute();
		for (const auto &entry : config.entries) {
			// WHY: a chat that still has a live server-side repeat must never
			// also fire locally, or the message would be sent twice.
			if (entry.serverScheduled
				|| entry.text.trimmed().isEmpty()
				|| entry.lastSentDate == today) {
				continue;
			}
			// WHY: catch-up is limited to kCatchUpMinutes after the set time,
			// so the app never fires in the middle of the night.
			const auto delay = minute - entry.minuteOfDay;
			if (delay < 0 || delay > kCatchUpMinutes) {
				continue;
			}
			const auto peer = TargetPeer(_session, entry.peer);
			if (!peer || !Data::CanSendTexts(peer)) {
				continue;
			}
			Send(_session, peer, entry, 0);
			markSent(entry.peer, today);
		}
	}

	void markSent(const QString &id, const QString &today) {
		auto &options = ForAccount(_session);
		const auto config = ReadConfig(
			options.Get(ServiceSettings::kDailySchedules))
			.value_or(DailySchedules());
		auto ok = false;
		const auto serialized = id.toULongLong(&ok);
		if (!ok || !serialized) {
			return;
		}
		if (const auto updated = MarkSent(config, serialized, today)) {
			Expects(options.Set(
				ServiceSettings::kDailySchedules,
				SerializeConfig(*updated)));
		}
	}

	const gsl::not_null<Main::Session*> _session;
	QTimer _timer;

};

} // namespace

void ScheduleServerSide(
		gsl::not_null<Main::Session*> session,
		gsl::not_null<PeerData*> peer) {
	if (!session->premium() || !Data::CanSendTexts(peer)) {
		return;
	}
	const auto id = SerializePeerId(peer->id);
	auto &options = ForAccount(session);
	const auto config = ReadConfig(
		options.Get(ServiceSettings::kDailySchedules))
		.value_or(DailySchedules());
	const auto entry = FindEntry(config, id);
	// Never schedule twice: a second call would stack a second daily repeat.
	if (!entry || entry->serverScheduled || entry->text.trimmed().isEmpty()) {
		return;
	}
	Send(session, peer, *entry, NextOccurrence(entry->minuteOfDay));
	if (const auto updated = SetServerScheduled(config, id, true)) {
		Expects(options.Set(
			ServiceSettings::kDailySchedules,
			SerializeConfig(*updated)));
	}
}

void StartDailyScheduler(gsl::not_null<Main::Session*> session) {
	// WHY: no QObject parent, so the session lifetime below is the only owner;
	// two owners could double-delete the timer during teardown.
	const auto scheduler = new DailyScheduler(nullptr, session);
	session->lifetime().add([=] { delete scheduler; });
}

} // namespace Serein::Schedule
