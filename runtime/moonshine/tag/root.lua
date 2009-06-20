local Tag       = require "moonshine.tag"

local Root = Tag:new()

function Root:announce_to_parent(...)
	-- We have no parent.
	-- We are an orphan.
	-- Woe to us.
end

function Root:set_parent(...)
	error("Can't attach the root to something")
end

function Root:parent()
	return nil
end

function Root:path()
	return ""
end

function Root:name()
	return "(root)"
end

function Root:display_name()
	return "(status)"
end

return Root
