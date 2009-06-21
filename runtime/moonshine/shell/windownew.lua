local screen = require "moonshine.ui.screen.main"
local Window = require "moonshine.ui.window"
local log    = require "moonshine.log"

local protocol = require "moonshine.protocol"
local M = {}

M.spec = { "tag|t=g", 1}

function M.run(o, room)
	local win = Window:new{}
	log('debug', "new window ID %s", tostring(win))
	assert(win._activity == 0 or win._activity > 0)
	screen:add(win)
end

return M
