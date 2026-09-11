// This file is part of Telegram Desktop.
#pragma once

#include "ui/layers/generic_box.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Plugins {

void PluginsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller);

} // namespace Plugins
