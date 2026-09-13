PLUGIN = {
    name = "UI Menus & Privacy",
    description = "Контекстные меню для сообщений и чатов, нативные диалоги, буфер обмена и Ghost Mode (/ghost, /notyping, /noread)",
    version = "1.1.0",
    author = "timesplinter"
}

function on_enable()
    -- 1. Контекстное меню сообщения (правый клик по сообщению в истории)
    telegram.ui.add_message_action("📋 Скопировать ID и текст", function(msg)
        local formatted = string.format("[Msg #%s | Chat #%s]: %s", tostring(msg.id), tostring(msg.peer_id), msg.text or "")
        telegram.clipboard.set(formatted)
        telegram.ui.show_toast("Скопировано в буфер обмена!")
    end)

    telegram.ui.add_message_action("💬 Быстрая цитата в поле ввода", function(msg)
        local quote_prompt = "> " .. (msg.text or "")
        telegram.ui.prompt("Цитирование сообщения", "Введите комментарий...", quote_prompt, function(res)
            if res and #res > 0 then
                telegram.ui.set_input_text(res)
                telegram.ui.show_toast("Цитата помещена в строку ввода!")
            end
        end)
    end)

    -- 2. Контекстное меню чата (правый клик по диалогу в левом списке)
    telegram.ui.add_chat_action("ℹ️ Параметры чата", function(chat)
        local chat_type = "Обычная группа"
        if chat.is_channel then
            chat_type = "Канал / Супергруппа"
        elseif chat.is_user then
            chat_type = "Личный диалог"
        end

        local text = string.format("Название: %s\nID чата: %s\nКатегория: %s",
            chat.name or "Без имени",
            tostring(chat.id),
            chat_type
        )
        telegram.ui.alert("Сведения о чате", text)
    end)

    telegram.ui.add_chat_action("❓ Тестовый диалог подтверждения", function(chat)
        telegram.ui.confirm("Тест подтверждения", "Проверить модальное окно подтверждения для " .. (chat.name or "чата") .. "?", function(confirmed)
            if confirmed then
                telegram.ui.show_toast("Вы нажали: Подтвердить (Да)")
            else
                telegram.ui.show_toast("Вы нажали: Отмена")
            end
        end)
    end)

    telegram.ui.show_toast("Плагин UI Menus & Privacy активирован!")
end

function on_command(cmd, args)
    if cmd == "/ghost" or cmd == "ghost" then
        local current = telegram.privacy.is_ghost_mode()
        local new_val = not current
        if args and args:match("on") then
            new_val = true
        elseif args and args:match("off") then
            new_val = false
        end
        telegram.privacy.set_ghost_mode(new_val)
        if new_val then
            telegram.ui.show_toast("👻 Ghost Mode ВКЛЮЧЕН (скрыт набор и статус прочтения)")
        else
            telegram.ui.show_toast("👤 Ghost Mode ВЫКЛЮЧЕН (стандартный режим)")
        end
        return true
    elseif cmd == "/notyping" or cmd == "notyping" then
        local current = telegram.privacy.is_typing_blocked()
        local new_val = not current
        telegram.privacy.set_typing_blocked(new_val)
        telegram.ui.show_toast("Скрытие набора текста: " .. (new_val and "ВКЛЮЧЕНО" or "ВЫКЛЮЧЕНО"))
        return true
    elseif cmd == "/noread" or cmd == "noread" then
        local current = telegram.privacy.is_read_blocked()
        local new_val = not current
        telegram.privacy.set_read_blocked(new_val)
        telegram.ui.show_toast("Скрытие прочтения сообщений: " .. (new_val and "ВКЛЮЧЕНО" or "ВЫКЛЮЧЕНО"))
        return true
    elseif cmd == "/clip" or cmd == "clip" then
        local clip_text = telegram.clipboard.get()
        telegram.ui.show_toast("Буфер: " .. (clip_text:sub(1, 40) or ""))
        return true
    end
    return false
end
