all: lexer ast

SRCS := lexer.cpp logging.cpp stringprintf.cpp \
				ast.cpp utils.cpp \

OBJS := $(subst .cpp,.o,$(SRCS))

CC := g++
CFLAGS := -std=c++11

%.o : %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<
	
lexer: $(OBJS)
	$(CC) -o $@ $^

ast: $(OBJS)
	$(CC) -o $@ $^