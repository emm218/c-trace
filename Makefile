CC=clang
CFLAGS+=-Wall -Wextra -Werror
LDLIBS+=-lpng

SRC:=$(wildcard *.c)

debug: CFLAGS+=-g
debug: c-trace

release: CFLAGS+=-O2
release: clean c-trace

c-trace: $(SRC:.c=.o)

.depend/%.d: %.c
	@mkdir -p $(dir $@) 
	$(CC) $(CFLAGS) -MM $^ -MF $@

include $(patsubst %.c, .depend/%.d, $(SRC))

clean:
	rm -f *.o c-trace

.PHONY: clean
