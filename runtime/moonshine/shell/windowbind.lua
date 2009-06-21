local screen = require "moonshine.ui.screen.main"
local Window = require "moonshine.ui.window"
local TagRoot= require "moonshine.tag.root"
local log    = require "moonshine.log"

local protocol = require "moonshine.protocol"
local M = {}

M.spec = { }

function M.run(o, tagname)
	local basetag = TagRoot
	if screen.window:tag() then
		basetag = screen.window:tag()
	end
	
	local tag = basetag:resolve_path(tagname)
	log('debug', 'resolve %s against %s', tagname, basetag:display_name())
	assert(tag, "tag does not exist")
	screen.window:add_tag(tag)
end

return M
