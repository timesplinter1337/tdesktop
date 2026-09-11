-- ==========================================================
-- Telegram Desktop Lua Plugin: Auto Responder & Logger
-- ==========================================================

Plugin = {
    name = "Auto Responder",
    author = "Community",
    version = "1.2",
    description = "Автоответчик на личные сообщения и логирование"
}

function Plugin:on_enable()
    telegram.log("Автоответчик активирован")
end

function Plugin:on_message(msg)
    -- Не отвечаем на собственные исходящие сообщения
    if msg.out then
        return
    end

    -- Логируем полученные сообщения
    telegram.log("Сообщение от " .. tostring(msg.from_id) .. ": " .. msg.text)

    -- Если сообщение содержит команду .help
    if msg.text == ".help" then
        telegram.send_message(msg.peer_id, "Привет! Доступные команды Lua-плагина:\n.shrug\n.ping\n.lenny")
    end
end
