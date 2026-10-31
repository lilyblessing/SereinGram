#include "serein/filters/menu.h"

#include "serein/filters/model.h"
#include "serein/hooks/menu/actions.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"

#include "styles/style_menu_icons.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QUuid>
#include <algorithm>

namespace Serein::Filters {

void InsertAuthorAction(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller) {
	if (!menu || !item || !controller || !item->from()) {
		return;
	}
	const auto session = &controller->session();
	const auto author = QString::number(SerializePeerId(item->from()->id));
	const auto read = [=]() -> std::optional<FilterRules> {
		const auto raw = ForAccount(session).Get(kRules);
		return ReadRules(raw);
	};
	const auto config = read();
	if (!config) {
		return;
	}
	const auto &authors = config->hiddenAuthors;
	const auto hidden = std::find(authors.begin(), authors.end(), author)
		!= authors.end();
	const auto title = hidden
		? tr::lng_serein_filter_author_show(tr::now)
		: tr::lng_serein_filter_author_hide(tr::now);
	const auto action = Ui::Menu::CreateAction(menu, title,
		crl::guard(controller, [=] {
			auto updated = read();
			if (!updated) {
				controller->showToast(tr::lng_serein_filter_invalid(tr::now));
				return;
			}
			auto &list = updated->hiddenAuthors;
			if (const auto i = std::find(list.begin(), list.end(), author)
				; i != list.end()) {
				list.erase(i);
			} else {
				list.push_back(author);
				updated->enabled = true;
			}
			if (!ForAccount(session).Set(kRules, SerializeFilterRules(*updated))) {
				controller->showToast(tr::lng_serein_filter_invalid(tr::now));
				return;
			}
			controller->showToast(tr::lng_serein_filter_menu_saved(tr::now));
		}));
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action,
		&st::menuIconBlock, &st::menuIconBlock);
	auto position = int(menu->actions().size());
	for (auto index = 0; index != position; ++index) {
		const auto tag = menu->actions()[index]->property("sereinMenuActionId");
		if (tag.isValid() && tag.toInt() == int(Menu::ActionId::Delete)) {
			position = index;
			break;
		}
	}
	Menu::Tag(menu->insertAction(position, std::move(widget)),
		Menu::ActionId::FilterAuthor);
}

void InsertSelectionRuleAction(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller,
		const QString &selected) {
	if (!menu || !item || !controller || selected.isEmpty()) {
		return;
	}
	const auto session = &controller->session();
	const auto read = [=]() -> std::optional<FilterRules> {
		const auto raw = ForAccount(session).Get(kRules);
		return ReadRules(raw);
	};
	const auto config = read();
	if (!config) {
		return;
	}
	const auto title = selected.simplified().left(32).trimmed();
	if (title.isEmpty()) {
		return;
	}
	const auto action = Ui::Menu::CreateAction(menu,
		tr::lng_serein_filter_selection_add(tr::now),
		crl::guard(controller, [=] {
			auto updated = read();
			if (!updated) {
				controller->showToast(tr::lng_serein_filter_invalid(tr::now));
				return;
			}
			if (updated->rules.size() >= 32) {
				controller->showToast(
					tr::lng_serein_filter_rules_limit(tr::now));
				return;
			}
			const auto pattern = QRegularExpression::escape(selected);
			if (pattern.size() > 2048) {
				controller->showToast(
					tr::lng_serein_filter_pattern_long(tr::now));
				return;
			}
			updated->rules.push_back(FilterRule{
				.id = QUuid::createUuid().toString(QUuid::WithoutBraces),
				.title = title,
				.pattern = pattern,
				.enabled = true,
				.caseInsensitive = true,
				.action = u"hide"_q,
			});
			updated->enabled = true;
			if (!ValidFilterRules(*updated)
				|| !ForAccount(session).Set(
					kRules,
					SerializeFilterRules(*updated))) {
				controller->showToast(tr::lng_serein_filter_invalid(tr::now));
				return;
			}
			controller->showToast(tr::lng_serein_filter_menu_saved(tr::now));
		}));
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action,
		&st::menuIconBlock, &st::menuIconBlock);
	Menu::Tag(
		menu->insertAction(Menu::DeleteActionIndex(menu), std::move(widget)),
		Menu::ActionId::SelectionFilter);
}

} // namespace Serein::Filters
