-- wrk POST echo: 128B body, expect 128B response (keep-alive)
local payload_size = tonumber(os.getenv("PAYLOAD") or "128")
local fill = string.char(string.byte("E"))
local payload = string.rep(fill, payload_size)

request = function()
  return wrk.format("POST", "/echo", {
    ["Content-Type"] = "application/octet-stream",
    ["Content-Length"] = tostring(payload_size),
    ["Connection"] = "keep-alive",
  }, payload)
end

response = function(status, headers, body)
  if status ~= 200 then
    wrk.thread:stop()
    return
  end
  if #body ~= payload_size then
    wrk.thread:stop()
  end
end
