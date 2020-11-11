function Broadcast()
    local data = DataBuffer()
    data.msg = "sender"
    NewMessage("msg", data)
end

function Listen(id, data)
    return data.msg
end
