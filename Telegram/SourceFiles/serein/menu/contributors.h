#pragma once

#include "base/basic_types.h"
#include "serein/hooks/menu/selection.h"

#include <QString>
#include <vector>

class HistoryItem;
struct FullMsgId;

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Serein::Menu {

struct Context {
	not_null<Ui::PopupMenu*> menu;
	HistoryItem *item = nullptr;
	not_null<Window::SessionController*> controller;
	const std::vector<FullMsgId> &selected;
	SelectionTarget selection;
	QString selectedText;
};

using Contributor = Fn<void(const Context &context)>;

void AddContributor(Contributor contributor);
void Contribute(const Context &context);

} // namespace Serein::Menu
