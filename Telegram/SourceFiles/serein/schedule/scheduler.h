#pragma once

#include <gsl/pointers>

class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace Serein::Schedule {

void StartDailyScheduler(gsl::not_null<Main::Session*> session);

// WHY: only Premium can schedule a server-side daily repeat, so this is a
// no-op otherwise or when the chat is already scheduled.
void ScheduleServerSide(
	gsl::not_null<Main::Session*> session,
	gsl::not_null<PeerData*> peer);

} // namespace Serein::Schedule
