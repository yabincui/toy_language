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
	src/string.cpp \
	src/supportlib.cpp \
	src/utils.cpp \

SRCS := \
	src/main.cpp \
	src/lexer.cpp \
	src/supportlib.cpp \
	src/utils.cpp \
	src/string.cpp \
	src/logging.cpp \
	src/parse.cpp \
	src/code.cpp \
	src/optimization.cpp \
	src/execution.cpp \

OBJS := $(subst .cpp,.o,$(subst src/,$(OUT_DIR)/,$(SRCS)))

CC := clang++

LLVM_CXX_FLAGS := $(shell llvm-config --cxxflags)

CXXFLAGS := -std=c++11 $(LLVM_CXX_FLAGS) -fno-rtti -Wno-dangling-else

LLVM_LDFLAGS := $(shell llvm-config --ldflags --libs --system-libs)

LDFLAGS := $(LLVM_LDFLAGS)

DEPS := Makefile $(wildcard src/*.h)

$(OUT_DIR)/%.o : src/%.cpp $(DEPS)
	$(CC) $(CXXFLAGS) -c -o $@ $<
	
$(TARGET): format $(OUT_DIR) $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

format:
	src/format.sh

clean:
	rm -rf $(OUT_DIR)

runtest: $(TARGET)
	runtest/runtest.sh $(TARGET) runtest/runtest_input.txt out/runtest_output.txt runtest/runtest_std_output.txt

.PHONY: clean runtest