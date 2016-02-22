#!/usr/bin/env lua

local bench = require"bench"
local aux = require"bench-aux"

local lib = ... or "bench-wheel.so"
local limit = 1000000
local step  = limit / 100

local B = require"bench".new(lib, count)

for i=0,limit,step do
	local fill_t = aux.time(B.fill, B, i)
	local expire_t = aux.time(B.expire, B, i)--, 60000000)

	aux.say("%i\t%f\t(%f)", i, expire_t, fill_t)
end
