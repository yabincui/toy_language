TARGETS := toy

all: $(TARGETS)

SRCS := \
  parse.cpp \
  code.cpp \
  execution.cpp \
  lexer.cpp \
  logging.cpp \
  main.cpp \
  optimization.cpp \
  string.cpp \
  supportlib.cpp \
  utils.cpp \

OBJS := $(subst .cpp,.o,$(SRCS))

CC := clang++

LLVM_CXX_FLAGS := $(shell llvm-config --cxxflags)

CXXFLAGS := -std=c++11 $(LLVM_CXX_FLAGS) -fno-rtti

LLVM_LDFLAGS := $(shell llvm-config --ldflags --libs --system-libs)

LDFLAGS := $(LLVM_LDFLAGS)

DEPS := Makefile $(wildcard *.h)

%.o : %.cpp $(DEPS)
	$(CC) $(CXXFLAGS) -c -o $@ $<
	
$(TARGETS): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	
clean: $(DEPS)
	rm -rf $(OBJS) $(TARGETS)