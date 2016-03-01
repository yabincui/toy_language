#include "option.h"

#include <errno.h>
#include <string.h>

#include <fstream>

#include "code.h"
#include "compilation.h"
#include "execution.h"
#include "lexer.h"
#include "logging.h"
#include "optimization.h"
#include "parse.h"
#include "strings.h"
#include "supportlib.h"

static void usage(const std::string& exec_name) {
  printf("%s  Experiment a toy language\n", exec_name.c_str());
  printf(
      "Usage:\n"
      "-c <file>       Compile the code into object file.\n"
      "-s <file>       Compile the code into assembly file.\n"
      "--dump dumpType1, dumpType2,...\n"
      "                Dump specified contents. Possible type list:\n"
      "                  token:  Dump all tokens received.\n"
      "                  ast:    Dump abstract syntax tree.\n"
      "                  code:   Dump generated IR code.\n"
      "                  none:   Don't dump any thing.\n"
      "-h/--help       Print this help information.\n"
      "-i <file>       Read input from specified file instead of standard\n"
      "                input.\n"
      "-o <file>       Write output to specified file instead of standard\n"
      "                output.\n"
      "--log <log_level>\n"
      "                Set log level, can be debug/info/error/fatal.\n"
      "                Default is debug.\n"
      "--no-execute    Don't execute code.\n"
      "Default Option: --dump code\n\n");
}

bool nextArgumentOrError(const std::vector<std::string>& Args, size_t& i) {
  if (i == Args.size() - 1) {
    LOG(ERROR) << "No argument following " << Args[i] << " option.";
    return false;
  }
  ++i;
  return true;
}

static std::ifstream ifs;
static std::ofstream ofs;

static bool parseOptions(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 0; i < argc; ++i) {
    args.push_back(argv[i]);
  }
  global_option.interactive = true;
  global_option.execute = true;
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-c") {
      if (!nextArgumentOrError(args, i)) {
        return false;
      }
      global_option.compile = true;
      global_option.compile_output_file = args[i];
    } else if (args[i] == "--dump") {
      if (!nextArgumentOrError(args, i)) {
        return false;
      }
      global_option.dump_token = false;
      global_option.dump_ast = false;
      global_option.dump_code = false;
      std::vector<std::string> dump_list = stringSplit(args[i], ',');
      for (const auto& item : dump_list) {
        if (item == "token") {
          global_option.dump_token = true;
        } else if (item == "ast") {
          global_option.dump_ast = true;
        } else if (item == "code") {
          global_option.dump_code = true;
        } else if (item == "none") {
        } else {
          LOG(ERROR) << "Unknown dump type " << item;
          return false;
        }
      }
    } else if (args[i] == "-h" || args[i] == "--help") {
      usage(args[0]);
      exit(0);
    } else if (args[i] == "-i") {
      if (!nextArgumentOrError(args, i)) {
        return false;
      }
      ifs.open(args[i].c_str());
      if (!ifs.good()) {
        LOG(ERROR) << "Can't open file " << args[i] << ": " << strerror(errno);
      }
      global_option.input_file = args[i];
      global_option.in_stream = &ifs;
      global_option.interactive = false;
    } else if (args[i] == "--log") {
      if (!nextArgumentOrError(args, i)) {
        return false;
      }
      if (args[i] == "debug") {
        global_option.log_level = DEBUG;
      } else if (args[i] == "error") {
        global_option.log_level = ERROR;
      } else if (args[i] == "fatal") {
        global_option.log_level = FATAL;
      } else {
        LOG(ERROR) << "Unknown log level: " << args[i];
        return false;
      }
    } else if (args[i] == "--no-execute") {
      global_option.execute = false;
    } else if (args[i] == "-o") {
      if (!nextArgumentOrError(args, i)) {
        return false;
      }
      ofs.open(args[i].c_str());
      if (!ofs.good()) {
        LOG(ERROR) << "Can't open file " << args[i] << ": " << strerror(errno);
        return false;
      }
      global_option.output_file = args[i];
      global_option.out_stream = &ofs;
    } else if (args[i] == "-s") {
      if (!nextArgumentOrError(args, i)) {
        return false;
      }
      global_option.compile_assembly = true;
      global_option.compile_assembly_output_file = args[i];
    } else {
      LOG(ERROR) << "Unknown Option: " << args[i];
      return false;
    }
  }
  if (global_option.compile && global_option.interactive) {
    LOG(ERROR) << "Toy can't compile while being interactive\n";
    return false;
  }

  LOG(DEBUG) << global_option.str();
  return true;
}

static void interactiveMain() {
  prepareParsePipeline();
  prepareCodePipeline();
  prepareOptPipeline();
  prepareExecutionPipeline();

  printPrompt();
  while (true) {
    ExprAST* expr = parsePipeline();
    if (expr != nullptr) {
      std::unique_ptr<llvm::Module> module = codePipeline(expr);
      if (module != nullptr) {
        optPipeline(module.get());
        executionPipeline(module.release());
      }
    } else {
      Token curr = currToken();
      if (curr.type == TOKEN_EOF) {
        break;
      }
    }
  }

  finishExecutionPipeline();
  finishCodePipeline();
  finishOptPipeline();
  finishParsePipeline();
}

static void nonInteractiveMain() {
  LOG(DEBUG) << "parseMain()";
  std::vector<ExprAST*> exprs = parseMain();
  LOG(DEBUG) << "codeMain()";
  std::unique_ptr<llvm::Module> module = codeMain(exprs);
  LOG(DEBUG) << "optMain()";
  optMain(module.get());
  if (global_option.compile_assembly) {
    bool ret = compileMain(module.get(), true, global_option.compile_assembly_output_file);
    LOG(DEBUG) << "compileMain() assembly file -> " << ret;
  }
  if (global_option.compile) {
    bool ret = compileMain(module.get(), false, global_option.compile_output_file);
    LOG(DEBUG) << "compileMain() object file -> " << ret;
  }
  LOG(DEBUG) << "executionMain()";
  executionMain(module.release());
}

int main(int argc, char** argv) {
  if (!parseOptions(argc, argv)) {
    return -1;
  }

  initSupportLib();
  if (global_option.interactive) {
    interactiveMain();
  } else {
    nonInteractiveMain();
  }
  return 0;
}
