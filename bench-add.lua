#!/usr/bin/env lua

local lib = ... or "bench-wheel.so"

local limit = 1000000
local step  = limit / 100
local bench = require"bench".new(lib, count)
local clock = require"bench".clock

bench:fill(limit)
local n = limit

for i=0,limit,step do
	bench:expire(n)

	local start = clock()
	bench:fill(i)
	n = i
	local stop = clock()

	print(i, math.floor((stop - start) * 1000000))
end

