all: lexer ast code

SRCS := lexer.cpp logging.cpp stringprintf.cpp \
				ast.cpp utils.cpp \

OBJS := $(subst .cpp,.o,$(SRCS))

CC := g++
CFLAGS := -std=c++11

LDFLAGS := -lLLVMCore -lLLVMSupport -lpthread -ltinfo -ldl

%.o : %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<
	
lexer: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

ast: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	
code: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)