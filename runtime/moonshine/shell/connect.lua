local protocol = require "moonshine.protocol"
local M = {}

M.spec = { "protocol|p=s", 1, 2}

function M.run(o, host, port)
	local proto = o.protocol or 'irc'

	local tag = protocol.connect(proto, { hostname = host, port = port and tonumber(port) })
	o._window:add_tag(tag)
end

return M
