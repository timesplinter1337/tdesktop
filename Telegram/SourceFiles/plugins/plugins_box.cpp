// This file is part of Telegram Desktop.
#include "plugins/plugins_box.h"
#include "plugins/plugin_manager.h"
#include "plugins/lua_core.h"

#include <QDesktopServices>
#include <QUrl>

#include "ui/widgets/checkbox.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/vertical_list.h"
#include "window/window_session_controller.h"
#include "lang/lang_keys.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"

namespace Plugins {

void PluginsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(rpl::single(u"Плагины (Lua)"_q));

	const auto content = box->verticalLayout();

	const auto &core = LuaCore::Instance();
	const auto engineAvailable = core.isAvailable();

	// Engine status header
	const auto statusText = engineAvailable
		? u"Рантайм Lua 5.4 активен"_q
		: u"Внимание: lua54.dll не найден. Поместите lua54.dll в папку с Telegram.exe"_q;

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(statusText),
			st::boxLabel),
		st::boxRowPadding);

	Ui::AddSkip(content, st::boxLittleSkip);

	// Action buttons container
	const auto buttonsLayout = content->add(
		object_ptr<Ui::VerticalLayout>(content));

	const auto openFolderBtn = buttonsLayout->add(
		object_ptr<Ui::SettingsButton>(
			buttonsLayout,
			rpl::single(u"Открыть папку с плагинами"_q),
			st::settingsButton));

	openFolderBtn->setClickedCallback([=] {
		const auto path = PluginManager::Instance().pluginsDirectory();
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
	});

	const auto reloadBtn = buttonsLayout->add(
		object_ptr<Ui::SettingsButton>(
			buttonsLayout,
			rpl::single(u"Перезагрузить список плагинов"_q),
			st::settingsButton));

	Ui::AddSkip(content, st::boxLittleSkip);

	// Container for dynamic plugin list
	const auto listContainer = content->add(
		object_ptr<Ui::VerticalLayout>(content));

	const auto rebuildList = [=] {
		while (listContainer->count() > 0) {
			delete listContainer->widgetAt(0);
		}

		const auto &plugins = PluginManager::Instance().plugins();
		if (plugins.empty()) {
			listContainer->add(
				object_ptr<Ui::FlatLabel>(
					listContainer,
					rpl::single(u"Плагины не найдены. Поместите .lua файлы в папку plugins/"_q),
					st::boxLabel),
				st::boxRowPadding);
			return;
		}

		for (const auto &p : plugins) {
			const auto pluginId = p.id;
			const auto title = QString("%1 (v%2)")
				.arg(p.name)
				.arg(p.version.isEmpty() ? "1.0" : p.version);

			const auto check = listContainer->add(
				object_ptr<Ui::Checkbox>(
					listContainer,
					title,
					p.enabled,
					st::defaultCheckbox),
				st::boxRowPadding);

			check->checkedChanges(
			) | rpl::on_next([=](bool checked) {
				PluginManager::Instance().setPluginEnabled(pluginId, checked);
			}, check->lifetime());

			if (!p.description.isEmpty()) {
				const auto desc = QString("  Автор: %1 | %2")
					.arg(p.author.isEmpty() ? "Аноним" : p.author)
					.arg(p.description);

				listContainer->add(
					object_ptr<Ui::FlatLabel>(
						listContainer,
						rpl::single(desc),
						st::boxLabel),
					st::boxRowPadding);
			}

			if (!p.lastError.isEmpty()) {
				listContainer->add(
					object_ptr<Ui::FlatLabel>(
						listContainer,
						rpl::single(u"Ошибка: "_q + p.lastError),
						st::boxLabel),
					st::boxRowPadding);
			}

			Ui::AddSkip(listContainer, st::boxLittleSkip);
		}
		listContainer->resizeToWidth(box->width());
	};

	reloadBtn->setClickedCallback([=] {
		PluginManager::Instance().reloadPlugins();
		rebuildList();
	});

	// Initial load and render
	PluginManager::Instance().reloadPlugins();
	rebuildList();

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	box->setWidth(st::boxWideWidth);
}

} // namespace Plugins
