local M = {}
local TagRoot   = require "moonshine.tag.root"

function M.connect(name, ...)
	local log    = require "moonshine.log"
	local Class  = require("moonshine.protocol." .. name)
	local protocol = Class:new(...)
	local tag    = protocol:attach(TagRoot)

	protocol:connect()

	return tag
end

local METHODS = { "join", "part", "quit", "public_message", "private_message", "disconnect" }

for _, name in ipairs(METHODS) do
	M[name] = function(tag, ...)
		local proto = tag._protocol
		local func
		if proto then
			func = proto[name]
		end
		assert(func, "Bad command for this protocol")
		return func(proto, ...)
	end
end

return M
