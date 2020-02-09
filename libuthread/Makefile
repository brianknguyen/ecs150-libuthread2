# Static lib resources: http://tldp.org/HOWTO/Program-Library-HOWTO/static-libraries.html
# Dependencies resource: https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html#Preprocessor-Options

# Target library
# test_queue
lib := libuthread.a
objs := queue.o uthread.o context.o preempt.o

CC := gcc
CFLAGS := -Wall -Wextra -Werror
LIBC := ar
LIBFLAGS := rcs

ifneq ($(V),1)
Q = @
endif

all: $(lib) $(tests)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

libuthread.a: $(objs)
	@echo "AR $@"
	$(Q)$(LIBC) $(LIBFLAGS) $@ $^

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	rm -f $(lib) $(objs) $(deps) $(testObjs:.o=.d)
