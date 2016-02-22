#!/usr/bin/env lua

local bench = require"bench"
local aux = require"bench-aux"

local lib = ... or "bench-wheel.so"
local limit = 1000000
local step  = limit / 100

local B = bench.new(lib, count)

for i=0,limit,step do
	local fill_t = aux.time(B.fill, B, i, 60 * 1000000)
	local del_t = aux.time(B.del, B, 0, i)

	aux.say("%i\t%f\t(%f)", i, del_t, fill_t)
end
