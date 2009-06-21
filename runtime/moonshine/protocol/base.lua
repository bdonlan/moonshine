local prefs    = require "moonshine.prefs"
local Client   = require "moonshine.net.client"
local Object   = require "moonshine.object"
local Tag      = require "moonshine.tag"
local Protocol = Object:clone()

local NOTAG = "-NOTAG-"

Protocol:add_attribute('username')
Protocol:add_attribute('tag')

function Protocol:__new()
	assert(self:username(), "username required")
end

function Protocol:make_tagname(i)
	assert(getmetatable(self))
	return 'protocol' .. tostring(i+1)
end

function Protocol:vivify(name)
	return nil
end

function Protocol:make_tag(tagname)
	local tag = Tag:new{name = tagname}
	tag._protocol = self
	tag.vivify = function (tagself, name)
		return self:vivify(name)
	end
	return tag
end

function Protocol:attach(env)
	assert(getmetatable(self))

	-- find unused tag, using self:make_tag()
	local i = 0
	repeat
		tagname = self:make_tagname(i)
		i   = i + 1
	until not env:lookup(tagname)

	local tag = self:make_tag(tagname)
	env:attach(tag)
	self:tag(tag)

	return tag
end

function Protocol:detach(env)
	assert(getmetatable(self))

	-- remove protocol from environment.
	env[self:tag()] = nil

	-- set tag to NOTAG, invalid tag constant.
	self:tag(NOTAG)
end

-- trigger a hook, with tag context.
function Protocol:trigger(name, ...)
	self:tag():announce(name, {...})
	run_hook(name, self:tag(), ...)
end

return Protocol
