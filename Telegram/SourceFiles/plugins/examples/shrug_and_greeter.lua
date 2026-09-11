-- ==========================================================
-- Telegram Desktop Lua Plugin: Auto Shrug & Greeter
-- ==========================================================

Plugin = {
    name = "Auto Shrug & Greeter",
    author = "Community",
    version = "1.0",
    description = "Заменяет команду .shrug на эмодзи и реагирует на приветствия"
}

function Plugin:on_enable()
    telegram.log("Плагин " .. self.name .. " успешно включен!")
end

-- Хук на отправку сообщений (pre-send hook)
-- Вызывается перед тем, как сообщение уйдет на сервер.
-- Возвращает модифицированный текст сообщения.
function Plugin:on_pre_send(text, peer_id)
    -- Быстрая вставка shrug
    if text == ".shrug" then
        return "¯\\_(ツ)_/¯"
    end
    
    -- Пример команды пинга
    if text == ".ping" then
        return "Pong! (отправлено через Lua-плагин)"
    end

    -- Пример шаблона
    if text == ".lenny" then
        return "( ͡° ͜ʖ ͡°)"
    end

    return text
end

-- Хук на входящие сообщения (incoming message hook)
function Plugin:on_message(msg)
    -- Если сообщение не от нас и содержит слово 'привет'
    if not msg.out and string.find(string.lower(msg.text), "привет") then
        telegram.show_toast("Получено приветствие в чате ID: " .. tostring(msg.peer_id))
    end
end
