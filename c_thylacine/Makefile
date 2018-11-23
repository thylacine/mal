CFLAGS += -std=c11 -pedantic
CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -Werror
CFLAGS += -g
LDFLAGS += -g

# CFLAGS += -D_GNU_SOURCE # linux needs this for getopt and strdup

CFLAGS += -DHAVE_READLINE
LDFLAGS += -lreadline

#####################

TESTS =

SOURCES_BASE = types.h types.c \
               reader.h reader.c \
               printer.h printer.c \
               tokenizer.h tokenizer.c \
               strbuf.h strbuf.c \
               repl_env.h repl_env.c \
               logger.h logger.c \
               e.h e.c
#               interop.h interop.c
SOURCES_LISP = env.h env.c core.h core.c mal.h stepA_mal.c
SOURCES = $(SOURCES_BASE) $(SOURCES_LISP)

#####################

SRCS = step0_repl.c step1_read_print.c step2_eval.c step3_env.c \
       step4_if_fn_do.c step5_tco.c step6_file.c step7_quote.c \
       step8_macros.c step9_try.c stepA_mal.c
OBJS = $(SRCS:%.c=%.o)
BINS = $(OBJS:%.o=%)

OTHER_OBJS = console_input.o strbuf.o tokenizer.o core.o repl_env.o types.o reader.o printer.o env.o e.o logger.o
OTHER_HDRS = console_input.h strbuf.h tokenizer.h core.h repl_env.h types.h reader.h printer.h env.h e.h logger.h

LDFLAGS += -lgc

#####################

all: $(BINS)

dist: mal

mal: $(word $(words $(BINS)),$(BINS))
	cp $< $@

$(OBJS) $(OTHER_OBJS): %.o: %.c  $(OTHER_HDRS)
	$(CC) $(CFLAGS) -c $(@:%.o=%.c) -o $@

$(patsubst %.o,%,$(filter step%,$(OBJS))): $(OTHER_OBJS)
$(BINS): %: %.o
	$(CC) $+ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(BINS) $(OTHER_OBJS) mal

.PHONY: stats stats-lisp tests $(TESTS)

stats: $(SOURCES)
	@wc $^
	@printf "%5s %5s %5s %s\n" `grep -E "^[[:space:]]*//|^[[:space:]]*$$" $^ | wc` "[comments/blanks]"
stats-lisp: $(SOURCES_LISP)
	@wc $^
	@printf "%5s %5s %5s %s\n" `grep -E "^[[:space:]]*//|^[[:space:]]*$$" $^ | wc` "[comments/blanks]"

tests: $(TESTS)

$(TESTS):
	@echo "Running $@"; \
	./$@ || exit 1; \
