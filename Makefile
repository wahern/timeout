# NOTE: GNU Make 3.81 won't export MAKEFLAGS if .POSIX is specified, but
# Solaris make won't export MAKEFLAGS unless .POSIX is specified.
$(firstword ignore).POSIX:

.DEFAULT_GOAL = all

.SUFFIXES:

all:

#
# USER-MODIFIABLE MACROS
#
top_srcdir = .
top_builddir = .

CFLAGS = -O2 -march=native -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
SOFLAGS = $$(auto_soflags)
LIBS = $$(auto_libs)

ALL_CPPFLAGS = -I$(top_srcdir) -DWHEEL_BIT=$(WHEEL_BIT) -DWHEEL_NUM=$(WHEEL_NUM) $(CPPFLAGS)
ALL_CFLAGS = $(CFLAGS)
ALL_SOFLAGS = $(SOFLAGS)
ALL_LDFLAGS = $(LDFLAGS)
ALL_LIBS = $(LIBS)

LUA_API = 5.3
LUA = lua
LUA51_CPPFLAGS = $(LUA_CPPFLAGS)
LUA52_CPPFLAGS = $(LUA_CPPFLAGS)
LUA53_CPPFLAGS = $(LUA_CPPFLAGS)

WHEEL_BIT = 6
WHEEL_NUM = 4

RM = rm -f

# END MACROS

SHRC = \
	top_srcdir="$(top_srcdir)"; \
	top_builddir="$(top_builddir)"; \
	. "$${top_srcdir}/Rules.shrc"

LUA_APIS = 5.1 5.2 5.3

include $(top_srcdir)/lua/Rules.mk
include $(top_srcdir)/bench/Rules.mk

all: check-timeout-cb test-timeout

check-timeout-cb:
	@echo $(CFLAGS) $(CPPFLAGS) | grep "\-DTIMEOUT_CB_OVERRIDE" > /dev/null  \
	&& NEED_OVERRIDE=1 || NEED_OVERRIDE=0;\
	if [[ "$${NEED_OVERRIDE}" -eq "1" ]];then \
	TEMPOUT=`mktemp /tmp/timeout-out.XXXXXX`; \
	TEMPCHK=`mktemp /tmp/timeout-chk.XXXXXX`; \
	printf "#include \"timeout_cb.h\"\nint main(){struct timeout_cb cb;return 0;}" > $${TEMPCHK}.c; \
	$(CC) -o $${TEMPOUT} $${TEMPCHK}.c $(ALL_CPPFLAGS) 2> /dev/null && HAS_TIMEOUT_CB=1 || HAS_TIMEOUT_CB=0; \
	$(RM) /tmp/timeout-out.* /tmp/timeout-chk.*; \
	if [[ "$${HAS_TIMEOUT_CB}" -eq "0" ]];then echo "Error: Can not find 'timeout_cb.h' or have not defined \
	struct timeout_cb. If you have defined the macro TIMEOUT_CB_OVERRIDE, please create 'timeout_cb.h' and \
	define your own struct timeout_cb in it." && exit 1; fi fi

timeout.o: $(top_srcdir)/timeout.c
test-timeout.o: $(top_srcdir)/test-timeout.c

timeout.o test-timeout.o:
	@$(SHRC); echo_cmd $(CC) $(ALL_CFLAGS) -c -o $@ $${top_srcdir}/$(@F:%.o=%.c) $(ALL_CPPFLAGS)

test-timeout: timeout.o test-timeout.o
	@$(SHRC); echo_cmd $(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -o $@ timeout.o test-timeout.o

.PHONY: clean clean~

clean:
	$(RM) $(top_builddir)/test-timeout $(top_builddir)/*.o
	$(RM) -r $(top_builddir)/*.dSYM

clean~:
	find $(top_builddir) $(top_srcdir) -name "*~" -exec $(RM) -- {} "+"
