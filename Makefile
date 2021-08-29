RM        = @-rm -f
CC        = gcc
CFLAGS    = -g -Wall -Werror

EXEC      = mping
OBJS      = mping.o
SRCS      = $(OBJS:.o=.c)
DEPS      = $(addprefix .,$(SRCS:.c=.d))

# Autodependecy generation via GCC -M.
.%.d: %.c
	@$(SHELL) -ec '$(CC) -MM $(CFLAGS) $(CPPFLAGS) $< 2>/dev/null \
                       | sed '\''s/\($*\)\.o[ :]*/\1.o $@ : /g'\'' > $@; \
                       [ -s $@ ] || rm -f $@'

all:	$(EXEC)

$(EXEC): $(OBJS)

clean:
	$(RM) mping $(EXEC) $(OBJS) $(DEPS)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif
