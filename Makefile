TARGETS := toy

all: $(TARGETS)

SRCS := \
  ast.cpp \
  code.cpp \
  execution.cpp \
  lexer.cpp \
  logging.cpp \
  main.cpp \
  optimization.cpp \
  string.cpp \
  utils.cpp \

OBJS := $(subst .cpp,.o,$(SRCS))

CC := clang++

LLVM_CXX_FLAGS := $(shell llvm-config --cxxflags)

CXXFLAGS := -std=c++11 $(CFLAGS_FOR_MACOS) $(CXX_INCLUDE_FLAGS) $(LLVM_CXX_FLAGS)

LLVM_LIBS := $(shell llvm-config --libs)

LLVM_LDFLAGS := `llvm-config --ldflags --libs --system-libs`

LDFLAGS := $(LLVM_LDFLAGS) -lpthread -ldl -lncurses

DEPS := Makefile $(wildcard *.h)

%.o : %.cpp $(DEPS)
	$(CC) $(CXXFLAGS) -c -o $@ $<
	
$(TARGETS): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	
clean: $(DEPS)
	rm -rf $(OBJS) $(TARGETS)