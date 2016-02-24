CPPFLAGS = -DTIMEOUT_DEBUG
CFLAGS = -O2 -march=native -g -Wall -Wextra -Wno-unused-parameter
LUA = lua

all: bench.so bench-wheel.so bench-heap.so bench-llrb.so test-timeout

WHEEL_BIT = 6
WHEEL_NUM = 4

timeout: CPPFLAGS+=-DWHEEL_BIT=$(WHEEL_BIT) -DWHEEL_NUM=$(WHEEL_NUM)

timeout8: CPPFLAGS+=-DWHEEL_BIT=3 -DWHEEL_NUM=$(WHEEL_NUM)

timeout16: CPPFLAGS+=-DWHEEL_BIT=4 -DWHEEL_NUM=$(WHEEL_NUM)

timeout32: CPPFLAGS+=-DWHEEL_BIT=5 -DWHEEL_NUM=$(WHEEL_NUM)

timeout64: CPPFLAGS+=-DWHEEL_BIT=6 -DWHEEL_NUM=$(WHEEL_NUM)

timeout64 timeout32 timeout16 timeout8 timeout: timeout.c
	$(CC) $(CFLAGS) -o $@ $^ $(CPPFLAGS)

timeout.o: CPPFLAGS=-DWHEEL_BIT=$(WHEEL_BIT) -DWHEEL_NUM=$(WHEEL_NUM)

timeout.o: timeout.c
	$(CC) $(CFLAGS) -c -o $@ $^ $(CPPFLAGS)

bench: bench.c timeout.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< -ldl

test-timeout: timeout.o test-timeout.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ timeout.o test-timeout.o

ifeq ($(shell uname -s), Darwin)
SOFLAGS = -bundle -undefined dynamic_lookup
else
SOFLAGS = -fPIC -shared
endif

# so bench.so can load implementation module from CWD
LDFLAGS = -Wl,-rpath,.

# clock_gettime in librt.so
ifeq ($(shell uname -s), Linux)
LIBS = -lrt
endif


bench.so: bench.c
	$(CC) -o $@ $< $(CPPFLAGS) -DLUA_COMPAT_ALL $(CFLAGS) -Wno-unused-function $(SOFLAGS) $(LDFLAGS) $(LIBS)

bench-wheel8.so: CPPFLAGS+=-DWHEEL_BIT=3 -DWHEEL_NUM=$(WHEEL_NUM)

bench-wheel8.so: timeout.c

bench-wheel16.so: CPPFLAGS+=-DWHEEL_BIT=4 -DWHEEL_NUM=$(WHEEL_NUM)

bench-wheel16.so: timeout.c

bench-wheel32.so: CPPFLAGS+=-DWHEEL_BIT=5 -DWHEEL_NUM=$(WHEEL_NUM)

bench-wheel32.so: timeout.c

bench-wheel64.so: CPPFLAGS+=-DWHEEL_BIT=6 -DWHEEL_NUM=$(WHEEL_NUM)

bench-wheel64.so: timeout.c

bench-wheel%.so: bench-wheel.c timeout.h
	$(CC) -o $@ $< $(CPPFLAGS) $(CFLAGS) -Wno-unused-function $(SOFLAGS)


bench-wheel.so: CPPFLAGS+=-DWHEEL_BIT=$(WHEEL_BIT) -DWHEEL_NUM=$(WHEEL_NUM)

bench-wheel.so: timeout.c

bench-%.so: bench-%.c timeout.h
	$(CC) -o $@ $< $(CPPFLAGS) $(CFLAGS) -Wno-unused-function $(SOFLAGS)

%-add.dat: bench-%.so bench-add.lua bench-aux.lua bench.so
	$(LUA) bench-add.lua $< > $@.tmp
	mv $@.tmp $@

%-del.dat: bench-%.so bench-del.lua bench-aux.lua bench.so
	$(LUA) bench-del.lua $< > $@.tmp
	mv $@.tmp $@

%-expire.dat: bench-%.so bench-expire.lua bench-aux.lua bench.so
	$(LUA) bench-expire.lua $< > $@.tmp
	mv $@.tmp $@

bench.eps: bench.plt $(foreach OP, add del expire, wheel-$(OP).dat heap-$(OP).dat)
	gnuplot bench.plt > $@.tmp
	mv $@.tmp $@

bench.pdf: bench.eps
	ps2pdf $< $@

.PHONY: clean clean~

clean:
	$(RM) -r timeout timeout8 timeout16 timeout32 timeout64 *.dSYM *.so *.dat *.eps

clean~: clean
	$(RM) *~
