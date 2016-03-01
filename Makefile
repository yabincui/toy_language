OUT_DIR = out

SRCS := \
	src/code.cpp \
	src/compilation.cpp \
	src/execution.cpp \
	src/lexer.cpp \
	src/logging.cpp \
	src/main.cpp \
	src/optimization.cpp \
	src/option.cpp \
	src/parse.cpp \
	src/strings.cpp \
	src/supportlib.cpp \
	src/utils.cpp \

UNITTEST_SRCS := \
	unittest/gtest_main.cpp \
	unittest/script_test.cpp \

SUPPORTLIB_MAIN_SRCS := \
	src/supportlib_main.cpp \

OBJS := $(subst .cpp,.o,$(subst src/,$(OUT_DIR)/,$(SRCS)))
UNITTEST_OBJS := $(subst .cpp,.o,$(subst unittest/,$(OUT_DIR)/,$(UNITTEST_SRCS))) \
				 $(filter-out $(OUT_DIR)/main.o,$(OBJS))
SUPPORTLIB_MAIN_OBJS := $(subst .cpp,.o,$(subst src/,$(OUT_DIR)/,$(SUPPORTLIB_MAIN_SRCS)))

CC := g++

LLVM_CXX_FLAGS := $(shell llvm-config --cxxflags)

CXXFLAGS := -std=c++11 $(LLVM_CXX_FLAGS) -fno-rtti -Wno-dangling-else -g -Wno-parentheses

UNITTEST_CXXFLAGS := $(CXXFLAGS) -I unittest/gtest_src/include -I src/

LLVM_LDFLAGS := $(shell llvm-config --ldflags --libs --system-libs)

LDFLAGS := $(LLVM_LDFLAGS) -rdynamic

UNITTEST_LDFLAGS := $(LDFLAGS) -pthread -L$(OUT_DIR) -lgtest

DEPS := Makefile $(wildcard src/*.h)

TARGET := $(OUT_DIR)/toy
SUPPORTLIB_TARGET := $(SUPPORTLIB_MAIN_OBJS)

all: $(TARGET) $(SUPPORTLIB_TARGET)


$(OUT_DIR)/%.o : src/%.cpp $(DEPS)
	$(CC) $(CXXFLAGS) -c -o $@ $<

$(OUT_DIR)/%.o : unittest/%.cpp $(DEPS)
	$(CC) $(UNITTEST_CXXFLAGS) -c -o $@ $<
	
$(TARGET): format $(OUT_DIR) $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

format:
	src/format.sh

clean:
	rm -rf $(OUT_DIR)

$(OUT_DIR)/libgtest.a: Makefile
	g++ -isystem unittest/gtest_src/include -Iunittest/gtest_src -pthread \
		-c unittest/gtest_src/src/gtest-all.cc $(CXXFLAGS) -o $(OUT_DIR)/gtest-all.o
	ar -rv $@ $(OUT_DIR)/gtest-all.o

$(OUT_DIR)/unittest : $(UNITTEST_OBJS) $(OUT_DIR)/libgtest.a
	$(CC) -o $@ $^ $(UNITTEST_LDFLAGS)

unittest: format $(OUT_DIR) $(OUT_DIR)/unittest
	cp -r unittest/test_scripts $(OUT_DIR)
	$(OUT_DIR)/unittest

.PHONY: clean format unittest