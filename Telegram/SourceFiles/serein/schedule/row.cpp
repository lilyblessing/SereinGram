#include "serein/schedule/row.h"

#include "serein/core/options.h"
#include "serein/schedule/model.h"
#include "serein/schedule/scheduler.h"
#include "serein/schema/gen/config/schedule.h"
#include "serein/schema/gen/settings/services.h"
#include "base/basic_types.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "ui/layers/generic_box.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/time_input.h"
#include "ui/wrap/padding_wrap.h"
#include "window/window_session_controller.h"

#include <QtCore/QTime>

#include "styles/style_layers.h"
#include "styles/style_passcode_box.h"
#include "styles/style_settings.h"

namespace Serein::Schedule {
namespace {

constexpr auto kDefaultMinute = 9 * 60;
constexpr auto kMaximumTextLength = 4096;

[[nodiscard]] QString Describe(const DailySchedules &config, quint64 peer) {
	const auto entry = FindEntry(config, peer);
	if (!entry) {
		return tr::lng_serein_daily_schedule_off(tr::now);
	}
	const auto text = entry->text.trimmed();
	const auto shown = (text.size() > 24)
		? (text.left(23) + QChar(0x2026))
		: text;
	return FormatMinute(entry->minuteOfDay)
		+ (shown.isEmpty() ? QString() : (u" \u00b7 "_q + shown));
}

not_null<Ui::TimeInput*> AddTime(
		not_null<Ui::GenericBox*> box,
		int minute) {
	const auto row = box->addRow(object_ptr<Ui::FixedHeightWidget>(box));
	const auto input = Ui::CreateChild<Ui::TimeInput>(
		row,
		FormatMinute(minute),
		st::autolockTimeField,
		st::autolockDateField,
		st::scheduleTimeSeparator,
		st::scheduleTimeSeparatorPadding);
	input->resizeToWidth(st::autolockTimeWidth);
	row->widthValue() | rpl::on_next([=](int width) {
		row->resize(width, input->height());
		input->moveToRight(0, 0, width);
	}, row->lifetime());
	return input;
}

void ShowBox(
		gsl::not_null<Window::SessionController*> controller,
		gsl::not_null<PeerData*> peer) {
	const auto session = &peer->session();
	const auto id = SerializePeerId(peer->id);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_serein_daily_schedule());
		const auto config = ReadConfig(
			ForAccount(session).Get(ServiceSettings::kDailySchedules))
			.value_or(DailySchedules());
		const auto entry = FindEntry(config, id);
		const auto enabled = box->addRow(object_ptr<Ui::Checkbox>(
			box,
			tr::lng_serein_daily_schedule(),
			entry != nullptr));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_serein_daily_schedule_time(),
			st::boxLabel));
		const auto time = AddTime(
			box,
			entry ? entry->minuteOfDay : kDefaultMinute);
		const auto text = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::MultiLine,
			tr::lng_serein_daily_schedule_text(),
			entry ? entry->text : QString()));
		text->setMaxLength(kMaximumTextLength);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			(session->premium()
				? tr::lng_serein_daily_schedule_about_server()
				: tr::lng_serein_daily_schedule_about()),
			st::boxDividerLabel));

		box->addButton(tr::lng_settings_save(), [=] {
			const auto parsed = QTime::fromString(
				time->valueCurrent(),
				u"H:mm"_q);
			if (!parsed.isValid()) {
				time->showError();
				return;
			}
			auto &options = ForAccount(session);
			const auto current = ReadConfig(
				options.Get(ServiceSettings::kDailySchedules))
				.value_or(DailySchedules());
			const auto on = enabled->checked();
			const auto minute = parsed.hour() * 60 + parsed.minute();
			const auto updated = on
				? WithEntry(current, id, minute, text->getLastText())
				: WithoutEntry(current, id);
			if (!updated) {
				return;
			}
			auto saved = *updated;
			if (!(session->premium() && on)) {
				if (const auto cleared = SetServerScheduled(saved, id, false)) {
					saved = *cleared;
				}
			}
			Expects(options.Set(
				ServiceSettings::kDailySchedules,
				SerializeConfig(saved)));
			if (session->premium() && on) {
				ScheduleServerSide(session, peer);
			}
			box->closeBox();
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

} // namespace

void AddDailyScheduleRow(const ChatSettingsContext &context) {
	const auto session = &context.peer->session();
	const auto id = SerializePeerId(context.peer->id);
	::Settings::AddButtonWithLabel(
		context.container,
		tr::lng_serein_daily_schedule(),
		ForAccount(session).Value(ServiceSettings::kDailySchedules)
			| rpl::map([=](const QByteArray &raw) {
				return Describe(ReadConfig(raw).value_or(DailySchedules()), id);
			}),
		st::settingsButtonNoIcon
	)->setClickedCallback([=] {
		ShowBox(context.controller, context.peer);
	});
}

} // namespace Serein::Schedule
