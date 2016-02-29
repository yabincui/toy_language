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
  std::string input_mark = ">>>Input";
  std::string output_mark = ">>>Output";
  while (fgets(buf, sizeof(buf), fp.get()) != nullptr) {
    if (in_output) {
      expect_output->append(buf);
    } else if (in_input) {
      if (strncmp(buf, output_mark.c_str(), output_mark.size()) == 0) {
        in_output = true;
      } else {
        input->append(buf);
      }
    } else {
      if (strncmp(buf, input_mark.c_str(), input_mark.size()) == 0) {
        in_input = true;
      }
    }
  }
  return true;
}

Option global_option = {
    "<stdin>",  // input_file
    &std::cin,  // in_stream
    "<stdout>", // output_file
    &std::cout, // out_stream
    true,       // interactive
    false,      // dump_token
    false,      // dump_ast
    false,      // dump_code
    INFO,       // log_level
    true,       // execute
    "",         // compile_output_file
};

static bool executeScript(const std::string &script, std::string *output) {
  global_option.interactive = false;
  std::istringstream iss(script);
  global_option.input_file = "string";
  global_option.in_stream = &iss;
  std::ostringstream oss;
  global_option.output_file = "string";
  global_option.out_stream = &oss;
  std::vector<ExprAST *> exprs = parseMain();
  std::unique_ptr<llvm::Module> module = codeMain(exprs);
  optMain(module.get());
  executionMain(module.release());
  *output = oss.str();
  return true;
}

TEST(script_test, run_scripts) {
  std::vector<std::string> script_names;
  ASSERT_TRUE(enumerateTestScripts(&script_names));
  for (const auto &path : script_names) {
    GTEST_LOG_(INFO) << "Test script " << path;
    std::string input;
    std::string output;
    std::string expect_output;
    ASSERT_TRUE(readTestScript(path, &input, &expect_output));
    ASSERT_TRUE(executeScript(input, &output));
    ASSERT_EQ(expect_output, output);
    GTEST_LOG_(INFO) << "Test script " << path << " [OK]";
  }
}
