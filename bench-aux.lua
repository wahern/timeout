local bench = require"bench"
local clock = bench.clock

local aux = {}

local function time_return(begun, ...)
	local duration = clock() - begun
	return duration, ...
end

function aux.time(f, ...)
	local begun = clock()
	return time_return(begun, f(...))
end

function aux.say(...)
	print(string.format(...))
end

return aux
