local Object    = require "moonshine.object"
local Tag       = Object:clone()
local log       = require "moonshine.log"

-- Tags are arranged into a filesystem-like tree. Theoretically, an tag is
-- anything that can broadcast a message of interest to windows.

function Tag:vivify(name)
	-- In some tags, this might create a subtag automatically
	-- By default, we return nil - no autovivification
end

Tag:add_attribute("parent")
Tag:add_attribute("name")

Tag:before("name", function (self, newname) 
	if self:parent() and newname then
		error("Tried to rename tag while attached")
	end
end)

Tag.is_weak = false

function Tag:tag_table()
	local t = {}
	if self.is_weak then
		setmetatable(t, { __mode = 'v' })
	end

	function self:tag_table()
		return t
	end

	return self:tag_table()
end

function Tag:lookup(name, try_vivify)
	local tt = self:tag_table()
	local result = tt[name]

	-- try_vivify = nil will not trigger here; the default is thus true
	if result or try_vivify == false then
		return result
	end

	result = self:vivify(name)
	if result then
		assert(not result or result:name() == name)
		self:attach(result)
	end

	return result
end

function Tag:attach(value)
	local tt  = self:tag_table()
	local name = value:name()

	if value:parent() then
		error("Trying to attach a tag (%s/%s) that's already attached at %s",
			self:path(), name, value:path())
	end

	if tt[name] then
		error("Trying to overwrite a tag at %s", tt[name]:path())
	end
	
	value:parent(self)
	tt[name] = value

	assert(self:lookup(name) == value, "lookup works after attach")
	assert(self:resolve_path(name) == value, "resolve works after attach")
	assert(self:resolve_path(value:path()) == value, "resolve works on abs path")
	assert(value:parent() == self, "parent took")
end

function Tag:detach(tag)
	local tt  = self:tag_table()
	local name

	if type(tag) == "table" then
		name = tag:name()
		if tt[name] ~= tag then
			log('warning', "Tried to detach tag '%s' from %s but it wasn't attached", tag:name(), self:path())
			return
		end
	end

	local obj = tt[name]
	if obj == nil then
		return
	end

	tt[name] = nil
	obj:parent(nil)
end

function Tag:path()
	local parentpath = "(unlinked)"

	if self:parent() then
		parentpath = self:parent():path()
	end

	return parentpath .. "/" .. self:name()
end

-- Tag broadcasts

function Tag:broadcast_tbl()
	local bt = {
		active_ct = 0,
		callbacks = {}
	}
	function self:broadcast_tbl()
		return bt
	end
	return self:broadcast_tbl()
end

function Tag:register_listener(callback, is_active)
	local bt = self:broadcast_tbl()
	if bt.callbacks[callback] then
		error("registering a tag listener twice!")
	end
	bt.callbacks[callback] = is_active
	if is_active then
		bt.active_ct = bt.active_ct + 1
	end

	if bt.active_ct == 1 and self.on_active then
		self:on_active()
	end
end

function Tag:unregister_listener(callback)
	local bt = self:broadcast_tbl()
	if not bt.callbacks[callback] then
		log("warning", "Attempted to unregister a listener twice in tag %s", self:path())
	end

	if bt.callbacks[callback] then
		bt.active_ct = bt.active_ct - 1
	end

	bt.callbacks[callback] = nil

	if bt.active_ct == 0 and self.on_inactive then
		self:on_inactive()
	end
end

function Tag:announce(cmd, message, aux_info)
	local bt = self:broadcast_tbl()
	local consumed = false

	if aux_info == nil then
		aux_info = { origin = self }
	end

	for cb, active in pairs(bt.callbacks) do
		if cb(cmd, message, aux_info) and active then
			consumed = true
		end
	end

	aux_info.consumed = consumed

	self:announce_to_parent(cmd, message, aux_info)
end

function Tag:announce_to_parent(cmd, message, aux_info)
	aux_info.nonlocal = true
	if self:parent() then
		self:parent():announce(cmd, message, aux_info)
	end
end

function Tag:display_name()
	return self:path()
end

function Tag:dump_tree(prefix)
	if not prefix then
		log('debug', "*** DUMPING TAG TREE ***")
		prefix = ""
	end

	prefix = prefix .. "/" .. self:name()
	log('debug', prefix)
	for i, v in pairs(self:tag_table()) do
		v:dump_tree(prefix)
	end
end

local root
function Tag:resolve_path(path)
	if string.sub(path, 1, 1) == '/' then
		if not root then
			root = require "moonshine.tag.root"
		end
		local remain = string.sub(path, 2)
		return root:resolve_path(string.sub(path, 2))
	end

	local ptr = self
	for segment in string.gmatch(path, "[^/]+") do
		if segment == ".." then
			if ptr:parent() then
				ptr = ptr:parent()
			end
		else
			ptr = ptr:lookup(segment)
		end
		
		if not ptr then
			return nil
		end
	end

	return ptr
end

return Tag
