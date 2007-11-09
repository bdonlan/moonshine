require "object"

servers = {}

Haver = Object:new()

function Haver:init()
	self.hostname = self.hostname or 'lofn.sinedev.org'
	self.port     = self.port or 7575
	self.name     = self.hostname .. ":" .. self.port
	self.username = self.username or os.getenv('USER')

	net.connect(self.hostname, self.port, self:callback "on_connect")

	servers[self.name] = self
	screen:debug("Connecting to %1", self.name)
end

function Haver:shutdown()
	if self.handle then
		self.handle:close()
	end

	servers[self.name] = nil
end

function Haver:on_connect(fd, err)
	if fd then
		screen:debug("Connected to %1", self.name)
		self.reader = LineReader.new()
		self.handle = Handle.new(fd, self:callback "on_event")
		self:send("HAVER", "Moonshine/"..VERSION)
	else
		self:shutdown()
	end
end

function Haver:on_event(event, ...)
	if event == 'read' then
		local lines = self.reader:read(...)
		for i, line in ipairs(lines) do
			local msg = split("\t", line:gsub("\r$", ""))
			local cmd = table.remove(msg, 1):upper()
			if self[cmd] then
				self[cmd](self, unpack(msg))
			else
				screen:debug("Unknown command: %1", cmd)
			end
		end
	elseif event == 'eof' then
		screen:debug("eof")
		self:shutdown()
	elseif event == 'hup' then
		screen:debug("hup")
		self:shutdown()
	elseif event == 'error' then
		local err = ...
		screen:debug("error: %1[%2]: %3", err,domain, err,code, err.message)
		self:shutdown()
	end
end

function Haver:send(...)
	if self.handle then
		self.handle:write( join("\t", {...}) .. "\n" )
	end
end

function Haver:join(room) self:send('JOIN', room) end
function Haver:msg(target, msg)
	if target.type == 'room' then
		self:send('IN', target.name, 'say', msg)
	elseif target.type == 'user' then
		self:send('TO', target.name, 'say', msg)
	end
end


function Haver:HAVER(host, version, extensions)
	self.server_version = version
	self.extensions = split(",", extensions)
	
	screen:debug("trying to log in as %1", self.username)
	self:send('IDENT', self.username)
end

function Haver:HELLO(username, address)
	screen:debug("Logged in as %1", username)
end

function Haver:IN(room, user, type, msg)
	public_message_hook(self, room, user, type, msg)
end

function Haver:TO(user, type, msg)
	private_message_hook(self, user, type, msg)
end

function Haver:JOIN(room, user)
	join_hook(self, room, user)
end

function Haver:PART(room, user, ...)
	part_hook(self, room, user)
end


