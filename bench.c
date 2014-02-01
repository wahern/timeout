#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>

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
}; /* struct bench */


static int bench_new(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	size_t count = luaL_optlong(L, 2, 1000000);
	timeout_t tmax = luaL_optlong(L, 3, 60 * 1000);
	int verbose = (lua_isnone(L, 4))? 0 : lua_toboolean(L, 4);
	struct bench *B;
	struct benchops *ops;

	B = lua_newuserdata(L, sizeof *B);
	memset(B, 0, sizeof *B);

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
	size_t i;

	for (i = 0; i < B->count; i++) {
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
	{ NULL,     NULL }
};

static const luaL_Reg bench_metatable[] = {
	{ "__gc", &bench__gc },
	{ NULL,   NULL }
};

static const luaL_Reg bench_globals[] = {
	{ "new", &bench_new },
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



#if 0
int main(int argc, char **argv) {
	extern char *optarg;
	extern int optind;
	int optc;
	struct vops *vops;
	char cmd[256];
	struct op op;

	setlocale(LC_ALL, "C");

	while (-1 != (optc = getopt(argc, argv, SHORT_OPTS))) {
		switch (optc) {
		case 'n':
			MAIN.count = strtoul(optarg, NULL, 0);

			break;
		case 't':
			MAIN.maximum = (strtod(optarg, NULL) * TIMEOUT_mHZ);

			break;
		case 'v':
			MAIN.verbose++;

			break;
		case 'h':
			usage(stdout);

			return 0;
		default:
			usage(stderr);

			return 1;
		} /* switch() */
	} /* while() */

	argv += optind;
	argc -= optind;

	if (argc > 0) {
		MAIN.path = *argv++;
		--argc;
	}

	if (!MAIN.verbose)
		setvbuf(stdout, NULL, _IOFBF, 0);

	if (!(MAIN.timeout = calloc(MAIN.count, sizeof *MAIN.timeout)))
		err(1, "calloc");

	if (!(MAIN.solib = dlopen(MAIN.path, RTLD_NOW|RTLD_LOCAL)))
		errx(1, "%s: %s", MAIN.path, dlerror());

	if (!(vops = dlsym(MAIN.solib, "VOPS")))
		errx(1, "%s: %s", MAIN.path, dlerror());

	MAIN.vops = *vops;
	MAIN.vops.init(MAIN.timeout, MAIN.count, MAIN.verbose);

	while (fgets(cmd, sizeof cmd, stdin) && parseop(&op, cmd)) {
		struct timeout *to;
		unsigned n;

		switch (op.type) {
		case OP_QUIT:
			goto quit;
		case OP_HELP:
			usage(stdout);

			break;
		case OP_ADD:
			to = &MAIN.timeout[op.add.id];
			MAIN.vops.add(to, op.add.timeout);

			break;
		case OP_DEL:
			to = &MAIN.timeout[op.del.id];
			MAIN.vops.del(to);

			break;
		case OP_GET:
			n = 0;

			while ((to = MAIN.vops.get())) {
				if (op.get.verbose > 1)
					printf("#%ld expired (%llu >= %llu)\n", to - MAIN.timeout, to->expires, MAIN.curtime);
				n++;
			}

			if (op.get.verbose)
				printf("expired %u\n", n);

			break;
		case OP_STEP:
			MAIN.curtime += op.step.time;
			MAIN.vops.update(MAIN.curtime);

			break;
		case OP_UPDATE:
			MAIN.curtime = op.update.time;
			MAIN.vops.update(MAIN.curtime);

			break;
		case OP_CHECK:
			MAIN.vops.check();

			break;
		case OP_FILL:
			for (to = MAIN.timeout; to < &MAIN.timeout[MAIN.count]; to++) {
				MAIN.vops.add(to, arc4random_uniform(MAIN.maximum));
				//MAIN.vops.add(to, random() % MAIN.maximum);
			}

			break;
		case OP_TIME:
			printf("clock %ld\n", clock());

			break;
		case OP_NONE:
			break;
		case OP_OOPS:
			errx(1, "oops: %s", op.oops.why);

			break;
		}
	} /* while() */

quit:
	return 0;
} /* main() */

#endif
