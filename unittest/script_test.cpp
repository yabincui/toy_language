#include "gtest.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include <code.h>
#include <execution.h>
#include <logging.h>
#include <option.h>
#include <optimization.h>
#include <parse.h>

static bool enumerateTestScripts(std::vector<std::string> *script_names) {
  script_names->clear();
  std::string script_dir = getExecDir() + "/test_scripts/";
  std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(script_dir.c_str()),
                                                closedir);
  if (dir == nullptr) {
    LOG(ERROR) << "failed to open dir " << script_dir << ": "
               << strerror(errno);
    return false;
  }
  struct dirent *entry;
  while ((entry = readdir(dir.get())) != nullptr) {
    std::string tmp = entry->d_name;
    size_t pos = tmp.find(".test");
    if (pos != std::string::npos && pos + 5 == tmp.size()) {
      script_names->push_back(script_dir + entry->d_name);
    }
  }
  return true;
}

static bool readTestScript(const std::string &path, std::string *input,
                           std::string *expect_output) {
  std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(path.c_str(), "r"), fclose);
  if (fp == nullptr) {
    LOG(ERROR) << "failed to open file " << path << ": " << strerror(errno);
    return false;
  }
  input->clear();
  expect_output->clear();
  char buf[1024];
  bool in_input = false;
  bool in_output = false;
  std::string input_start_mark = ">>>Input Start";
  std::string input_end_mark = ">>>Input End";
  std::string output_start_mark = ">>>Output Start";
  std::string output_end_mark = ">>>Output End";
  while (fgets(buf, sizeof(buf), fp.get()) != nullptr) {
    if (strstr(buf, input_start_mark.c_str()) != nullptr) {
      in_input = true;
    } else if (strstr(buf, input_end_mark.c_str()) != nullptr) {
      in_input = false;
    } else if (strstr(buf, output_start_mark.c_str()) != nullptr) {
      in_output = true;
    } else if (strstr(buf, output_end_mark.c_str()) != nullptr) {
      in_output = false;
    } else if (in_input) {
      input->append(buf);
    } else if (in_output) {
      expect_output->append(buf);
    }
  }
  if (input->empty()) {
    LOG(ERROR) << "no input in file " << path;
    return false;
  }
  if (expect_output->empty()) {
    LOG(ERROR) << "no expected output in file " << path;
    return false;
  }
  return true;
}

static bool executeScript(const std::string &script, bool use_debug,
                          std::string *output) {
  global_option.execute = true;
  std::istringstream iss(script);
  global_option.input_file = "string";
  global_option.in_stream = &iss;
  std::ostringstream oss;
  global_option.output_file = "string";
  global_option.out_stream = &oss;
  global_option.debug = use_debug;
  std::vector<ExprAST *> exprs = parseMain();
  std::unique_ptr<llvm::Module> module = codeMain(exprs);
  optMain(module.get());
  executionMain(module.release());
  *output = oss.str();
  return true;
}

void runScripts(bool use_debug, bool *success) {
  *success = false;
  std::vector<std::string> script_names;
  ASSERT_TRUE(enumerateTestScripts(&script_names));
  for (const auto &path : script_names) {
    GTEST_LOG_(INFO) << "Test script " << path;
    std::string input;
    std::string output;
    std::string expect_output;
    ASSERT_TRUE(readTestScript(path, &input, &expect_output));
    ASSERT_TRUE(executeScript(input, use_debug, &output));
    ASSERT_EQ(expect_output, output);
    GTEST_LOG_(INFO) << "Test script " << path << " [OK]";
  }
  *success = true;
}

TEST(script_test, run_scripts) {
  bool success;
  runScripts(false, &success);
  ASSERT_TRUE(success);
}

TEST(script_test, run_scripts_debug) {
  bool success;
  runScripts(true, &success);
  ASSERT_TRUE(success);
}
