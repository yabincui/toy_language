OUT_DIR = out

TARGET := $(OUT_DIR)/toy

all: $(TARGET)

SRCS := \
	src/code.cpp \
	src/execution.cpp \
	src/lexer.cpp \
	src/logging.cpp \
	src/main.cpp \
	src/optimization.cpp \
	src/parse.cpp \
	src/strings.cpp \
	src/supportlib.cpp \
	src/utils.cpp \

UNITTEST_SRCS := \
	unittest/gtest_main.cpp \
	unittest/script_test.cpp \

OBJS := $(subst .cpp,.o,$(subst src/,$(OUT_DIR)/,$(SRCS)))
UNITTEST_OBJS := $(subst .cpp,.o,$(subst unittest/,$(OUT_DIR)/,$(UNITTEST_SRCS))) \
				 $(filter-out $(OUT_DIR)/main.o,$(OBJS))

CC := clang++

LLVM_CXX_FLAGS := $(shell llvm-config --cxxflags)

CXXFLAGS := -std=c++11 $(LLVM_CXX_FLAGS) -fno-rtti -Wno-dangling-else

UNITTEST_CXXFLAGS := $(CXXFLAGS) -I unittest/include -I src/

LLVM_LDFLAGS := $(shell llvm-config --ldflags --libs --system-libs)

LDFLAGS := $(LLVM_LDFLAGS)

UNITTEST_LDFLAGS := $(LDFLAGS) -pthread unittest/libgtest.a

DEPS := Makefile $(wildcard src/*.h)

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

$(OUT_DIR)/unittest : $(UNITTEST_OBJS)
	$(CC) -o $@ $^ $(UNITTEST_LDFLAGS)

unittest: format $(OUT_DIR)/unittest
	cp -r unittest/test_scripts $(OUT_DIR)
	$(OUT_DIR)/unittest

runtest: $(TARGET)
	runtest/runtest.sh $(TARGET) runtest/runtest_input.txt out/runtest_output.txt runtest/runtest_std_output.txt

.PHONY: clean runtest unittest