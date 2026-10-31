#pragma once

class HistoryItem;
namespace Ui { class PopupMenu; }
namespace Window { class SessionController; }

namespace Serein::Filters {

void InsertAuthorAction(
	Ui::PopupMenu *menu,
	HistoryItem *item,
	Window::SessionController *controller);

void InsertSelectionRuleAction(
	Ui::PopupMenu *menu,
	HistoryItem *item,
	Window::SessionController *controller,
	const QString &selected);

} // namespace Serein::Filters
