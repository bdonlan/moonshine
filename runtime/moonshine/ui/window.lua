local term      = require "moonshine.ui.term"
local Object    = require "moonshine.object"
local Buffer    = require "moonshine.ui.buffer"
local Statusbar = require "moonshine.ui.statusbar"
local Window    = Object:clone()

-- Window:add_attribute("name", { required = true })
-- For now, use the tag as our name. Later we may want name shortnames
-- (ie, #lobby vs sine/#lobby) or aliases... but this should be a function of
-- the tag
Window:add_attribute("tag")

function Window:name()
	local tag = self:tag()
	if tag then
		return tag
	else
		return "empty"
	end
end

function Window:__new()
	self._topic    = Statusbar:new("")
	self._buffer   = Buffer:new(1014)
	self._activity = 0
	self._alttags  = { }

	self:set_topic("Moonshine - A Haver Client")
end

function Window:add_tag(newtag)
	if self:tag() == newtag then
		return
	end

	local idx;
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
end

function Window:shift_tag()
	if tag ~= nil then
		table.insert(self._alttags, self:tag())
	end

	self:tag(self._alttags[1]);
	table.remove(self._alttags, 1)
end

function Window:print(fmt, ...)
	self:actprint(1, fmt, ...)
end

function Window:actprint(activity, fmt, ...)
	local str = term.format(os.date("%H:%M ")..tostring(fmt), { ... })

	self._activity = math.max(self._activity, activity)
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

return Window
