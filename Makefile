TARGETS := lexer ast code

all: $(TARGETS)

SRCS := lexer.cpp logging.cpp stringprintf.cpp \
				ast.cpp utils.cpp \

OBJS := $(subst .cpp,.o,$(SRCS))

CC := g++

CFLAGS_FOR_MACOS = -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS

CFLAGS := -std=c++11 $(CFLAGS_FOR_MACOS)

LDFLAGS := -lLLVMCore -lLLVMSupport -lpthread -ldl -lncurses

DEPS := Makefile $(wildcard *.h)

%.o : %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<
	
lexer: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

ast: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	
code: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	
clean: $(DEPS)
	rm -rf $(OBJS) $(TARGETS)