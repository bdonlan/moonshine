local term      = require "moonshine.ui.term"
local Object    = require "moonshine.object"
local Buffer    = require "moonshine.ui.buffer"
local Statusbar = require "moonshine.ui.statusbar"
local Tag       = require "moonshine.tag"
local TagRoot   = require "moonshine.tag.root"
local Window    = Object:clone()

-- Window:add_attribute("name", { required = true })
-- For now, use the tag as our name. Later we may want name shortnames
-- (ie, #lobby vs sine/#lobby) or aliases... but this should be a function of
-- the tag

Window:add_attribute("tag")
Window:add_attribute("activity")

local screen
function do_render()
	if not screen then
		screen = require "moonshine.ui.screen.main"
	end
	screen:render()
end

function Window:name()
	if not self:tag() then
		return "(empty)"
	else
		return self:tag():display_name()
	end
end

function Window:current_tag()
	return self:tag()
end

function Window:__new()
	self._topic    = Statusbar:new("")
	self._buffer   = Buffer:new(1014)
	self._alttags  = { }
	-- we need a unique value, so build a closure here
	self._callback = function (...)
		self:handle_broadcast(...)
	end

	self:activity(0)

	self:set_topic("Moonshine - A Haver Client")
end

function Window:handle_broadcast(trigger, args, aux_info)
	trigger = trigger:gsub(" ", "_")
	log('debug', 'broadcast trigger: ' .. trigger)
	if not aux_info.consumed and self["on_" .. trigger] then
		return self["on_" .. trigger](self, aux_info, unpack(args))
	end
	return false
end

function Window:add_tag(newtag)
	if self:tag() == newtag then
		return
	end

	local idx
	for i, v in ipairs(self._alttags) do
		if v == newtag then
			idx = i
			break
		end
	end

	if idx then return end

	table.insert(self._alttags, newtag)

	if not self:tag() then
		self:shift_tag()
	end

	newtag:register_listener(self._callback, true)
end

function Window:shift_tag()
	if self:tag() then
		table.insert(self._alttags, self:tag())
	end

	if self._alttags[1] then
		self:tag(self._alttags[1])
		table.remove(self._alttags, 1)
	end
end

-- TODO: remove_tag()

function Window:print(fmt, ...)
	self:actprint(1, fmt, ...)
end

function Window:actprint(activity, fmt, ...)
	local str = term.format(os.date("%H:%M ")..tostring(fmt), { ... })

	assert(activity ~= nil)
	self:activity(math.max(self:activity(), activity))
	assert(self:activity() ~= nil)
	self._buffer:print(str)
end

function Window:render(top, bottom)
	local rows, cols = term.dimensions()

	self._topic:render(top)
	if self._buffer:is_dirty() then
		self._buffer:render(top + 1, bottom)
	end
end

function Window:scroll(x)
	self._buffer:scroll(x)
end

function Window:set_topic(t)
	self._topic:set(term.format("%{topic}%1", { t }))
end

function Window:activate()
	self._buffer:is_dirty(true)
end

function Window:resize()
	self._buffer:is_dirty(true)
end

--- Announcement hooks
function Window:on_public_message(aux_info, deprecated_room, user, kind, text)
	local show_prefix = true

	if not self._alttags[1] and self:tag() and self:tag() == aux_info.origin then
		show_prefix = false
	end

	if show_prefix then
		self:print("[%1] <%2> %|%3", aux_info.origin:path(), user, text)
	else
		self:print("<%1> %|%2", user, text)
	end

	do_render()
end

return Window
