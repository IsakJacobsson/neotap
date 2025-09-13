CC = clang

PROG	= neotap
OBJS	= $(PROG).c parse_args.c parse_words.c stats.c
PROGS	= $(PROG)

CFLAGS += -Wall \
          -Wextra \
          -Wformat=2 \
          -Wpointer-arith \
          -Wbad-function-cast \
          -Wstrict-prototypes \
          -Wmissing-prototypes \
          -Winline \
          -Wdisabled-optimization \
          -Wfloat-equal \
          -W \
          -Werror

all:	$(PROGS)

$(PROG): $(OBJS)
	@$(CC) $^ $(CFLAGS) -o $@

clean:
	@rm -rf $(PROG)
