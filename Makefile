CC=clang
CFLAGS+=-Wall -Wextra -Werror -Wno-missing-field-initializers -Iinclude
LDLIBS+=-lpng -lm

SRC:=$(wildcard *.c)

debug: CFLAGS+=-g
debug: c-trace

release: CFLAGS+=-O2
release: clean keywords.c c-trace

c-trace: $(SRC:.c=.o) keywords.o

c-trace.o: CFLAGS+=-Wno-unused-function -Wno-unused-label 

keywords.c: keywords.gperf token.h scene.h
	gperf $< | clang-format > $@

keywords.o: CFLAGS+=-Wno-unused-parameter

compile_commands.json: Makefile
	$(CLEAN)
	bear -- make --no-print-directory --quiet

.depend/%.d: %.c
	@mkdir -p $(dir $@) 
	$(CC) $(CFLAGS) -MM $^ -MF $@

include $(patsubst %.c, .depend/%.d, $(SRC))

clean:
	rm -f *.o keywords.c c-trace

.PHONY: clean
