#!/usr/bin/env lua

local bench = require"bench"
local aux = require"bench-aux"

local lib = ... or "bench-wheel.so"
local limit = 1000000
local step  = limit / 100

local B = bench.new(lib, count)

B:fill(limit)
local n = limit

for i=0,limit,step do
	-- expire all timeouts
	local expire_t = aux.time(B.expire, B, n)

	-- add i timeouts
	local fill_t = aux.time(B.fill, B, i)
	n = i

	aux.say("%i\t%f\t(%f)", i, fill_t, expire_t)
end
