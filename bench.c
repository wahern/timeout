#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>

#if __APPLE__
#include <mach/mach_time.h>
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "timeout.h"
#include "bench.h"


struct bench {
	const char *path;
	void *solib;
	size_t count;
	timeout_t maximum;
	int verbose;

	void *state;
	struct timeout *timeout;
	struct benchops ops;
	timeout_t curtime;

#if __APPLE__
	mach_timebase_info_data_t timebase;
#endif
}; /* struct bench */


static int bench_clock(lua_State *L) {
#if __APPLE__
	struct bench *B = lua_touserdata(L, 1);
	unsigned long long abt;

	abt = mach_absolute_time();
	abt = abt * B->timebase.numer / B->timebase.denom;

	lua_pushnumber(L, (double)abt / 1000000000L);
#else
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	lua_pushnumber(L, (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000L));
#endif

	return 1;
} /* bench_clock() */


static int bench_new(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	size_t count = luaL_optlong(L, 2, 1000000);
	timeout_t tmax = luaL_optlong(L, 3, 60 * 1000);
	int verbose = (lua_isnone(L, 4))? 0 : lua_toboolean(L, 4);
	struct bench *B;
	struct benchops *ops;

	B = lua_newuserdata(L, sizeof *B);
	memset(B, 0, sizeof *B);

#if __APPLE__
	mach_timebase_info(&B->timebase);
#endif

	luaL_getmetatable(L, "BENCH*");
	lua_setmetatable(L, -2);

	B->count = count;
	B->maximum = tmax;
	B->verbose = verbose;

	if (!(B->timeout = calloc(count, sizeof *B->timeout)))
		return luaL_error(L, "%s", strerror(errno));

	if (!(B->solib = dlopen(path, RTLD_NOW|RTLD_LOCAL)))
		return luaL_error(L, "%s: %s", path, dlerror());

	if (!(ops = dlsym(B->solib, "benchops")))
		return luaL_error(L, "%s: %s", path, dlerror());

	B->ops = *ops;
	B->state = B->ops.init(B->timeout, B->count, B->verbose);

	return 1;
} /* bench_new() */


static int bench_add(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);
	unsigned i;
	timeout_t t;

	i = (lua_isnoneornil(L, 2))? random() % B->count : (unsigned)luaL_checklong(L, 2);
	t = (lua_isnoneornil(L, 3))? random() % B->maximum : (unsigned)luaL_checklong(L, 3);

	B->ops.add(B->state, &B->timeout[i], t);

	return 0;
} /* bench_add() */


static int bench_del(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);
	unsigned i;

	i = (lua_isnoneornil(L, 2))? random() % B->count : (unsigned)luaL_checklong(L, 2);

	B->ops.del(B->state, &B->timeout[i]);

	return 0;
} /* bench_del() */


static int bench_fill(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);
	size_t count = luaL_optlong(L, 2, B->count);
	size_t i;

	for (i = 0; i < count; i++) {
		B->ops.add(B->state, &B->timeout[i], random() % B->maximum);
	}

	return 0;
} /* bench_fill() */


static int bench_expire(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);
	unsigned count = luaL_optlong(L, 2, B->count);
	unsigned step = luaL_optlong(L, 3, 300);
	size_t i = 0;

	while (i < count && !B->ops.empty(B->state)) {
		B->curtime += step;
		B->ops.update(B->state, B->curtime);

		while (B->ops.get(B->state))
			i++;
	}

	return 0;
} /* bench_expire() */


static int bench__gc(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);

	if (B->state) {
		B->ops.destroy(B->state);
		B->state = NULL;
	}

	return 0;
} /* bench_expire() */


static const luaL_Reg bench_methods[] = {
	{ "add",    &bench_add },
	{ "del",    &bench_del },
	{ "fill",   &bench_fill },
	{ "expire", &bench_expire },
	{ "close",  &bench__gc },
	{ NULL,     NULL }
};

static const luaL_Reg bench_metatable[] = {
	{ "__gc", &bench__gc },
	{ NULL,   NULL }
};

static const luaL_Reg bench_globals[] = {
	{ "new",   &bench_new },
	{ "clock", &bench_clock },
	{ NULL,  NULL }
};

int luaopen_bench(lua_State *L) {
	if (luaL_newmetatable(L, "BENCH*")) {
		luaL_register(L, NULL, bench_metatable);
		lua_newtable(L);
		luaL_register(L, NULL, bench_methods);
		lua_setfield(L, -2, "__index");
	}

	lua_pop(L, 1);

	lua_newtable(L);
	luaL_register(L, NULL, bench_globals);

	return 1;
} /* luaopen_bench() */

