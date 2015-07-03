#include "option.h"

#include "code.h"
#include "execution.h"
#include "lexer.h"
#include "logging.h"
#include "parse.h"
#include "string.h"

static void usage(const std::string& ExecName) {
  printf("%s  Experiment a toy language\n", ExecName.c_str());
  printf("Usage:\n");
  printf("--dump dumpType1, dumpType2,...\n");
  printf("                Dump specified contents. Possible type list:\n");
  printf("                  token:  Dump all tokens received.\n");
  printf("                  ast:    Dump abstract syntax tree.\n");
  printf("                  code:   Dump generated IR code.\n");
  printf("                  none:   Don't dump any thing.\n");
  printf("-h/--help       Print this help information.\n");
  printf(
      "-i <file>       Read input from specified file instead of standard "
      "input.\n");
  printf(
      "--log <log_level>  Set log level, can be debug/info/error/fatal, "
      "default is debug.\n");
  printf("Default Option: --dump code\n");
}

Option GlobalOption = {
    "<stdin>",  // InputFile
    stdin,      // InputFp
    true,       // Interactive
    false,      // DumpToken
    false,      // DumpAST
    true,       // DumpCode
    INFO,       // LogLevel
};

bool nextArgumentOrError(const std::vector<std::string>& Args, size_t& i) {
  if (i == Args.size() - 1) {
    LOG(ERROR) << "No argument following " << Args[i] << " option.";
    return false;
  }
  ++i;
  return true;
}

static bool parseOptions(int argc, char** argv) {
  std::vector<std::string> Args;
  for (int i = 0; i < argc; ++i) {
    Args.push_back(argv[i]);
  }
  for (size_t i = 1; i < Args.size(); ++i) {
    if (Args[i] == "--dump") {
      if (!nextArgumentOrError(Args, i)) {
        return false;
      }
      GlobalOption.DumpToken = false;
      GlobalOption.DumpAST = false;
      GlobalOption.DumpCode = false;
      std::vector<std::string> DumpList = stringSplit(Args[i], ',');
      for (auto& Item : DumpList) {
        if (Item == "token") {
          GlobalOption.DumpToken = true;
        } else if (Item == "ast") {
          GlobalOption.DumpAST = true;
        } else if (Item == "code") {
          GlobalOption.DumpCode = true;
        } else if (Item == "none") {
        } else {
          LOG(ERROR) << "Unknown dump type " << Item;
          return false;
        }
      }
    } else if (Args[i] == "-h" || Args[i] == "--help") {
      usage(Args[0]);
      exit(0);
    } else if (Args[i] == "-i") {
      if (!nextArgumentOrError(Args, i)) {
        return false;
      }
      FILE* fp = fopen(Args[i].c_str(), "r");
      if (fp == nullptr) {
        LOG(ERROR) << "Can't open file " << Args[i];
        return false;
      }
      GlobalOption.InputFile = Args[i];
      GlobalOption.InputFp = fp;
      GlobalOption.Interactive = false;
    } else if (Args[i] == "--log") {
      if (!nextArgumentOrError(Args, i)) {
        return false;
      }
      if (Args[i] == "debug") {
        GlobalOption.LogLevel = DEBUG;
      } else if (Args[i] == "error") {
        GlobalOption.LogLevel = ERROR;
      } else if (Args[i] == "fatal") {
        GlobalOption.LogLevel = FATAL;
      } else {
        LOG(ERROR) << "Unknown log level: " << Args[i];
        return false;
      }
    } else {
      LOG(ERROR) << "Unknown Option: " << Args[i];
      return false;
    }
  }

  LOG(DEBUG) << "\n"
             << "GlobalOption: InputFile = " << GlobalOption.InputFile << "\n"
             << "              InputFp = " << GlobalOption.InputFp << "\n"
             << "              Interactive = " << GlobalOption.Interactive
             << "\n"
             << "              DumpToken = " << GlobalOption.DumpToken << "\n"
             << "              DumpAST = " << GlobalOption.DumpAST << "\n"
             << "              DumpCode = " << GlobalOption.DumpCode << "\n";
  return true;
}

void printPrompt() {
  printf(">");
  fflush(stdout);
}

static void interactiveMain() {
  prepareParsePipeline();
  std::unique_ptr<llvm::Module> Module = prepareCodePipeline();
  prepareExecutionPipeline(Module.release());

  printPrompt();
  while (true) {
    ExprAST* Expr = parsePipeline();
    if (Expr != nullptr) {
      llvm::Function* Function = codePipeline(Expr);
      if (Function != nullptr) {
        executionPipeline(Function);
      }
    }

    Token Curr = currToken();
    if (Curr.Type == TOKEN_EOF) {
      break;
    }
    ExprsInCurrLine++;
  }

  finishExecutionPipeline();
  finishCodePipeline();
  finishParsePipeline();
}

static void nonInteractiveMain() {
  std::vector<ExprAST*> Exprs = parseMain();
  std::unique_ptr<llvm::Module> Module = codeMain(Exprs);
  executionMain(Module.release());
}

int main(int argc, char** argv) {
  if (!parseOptions(argc, argv)) {
    return -1;
  }

  if (GlobalOption.Interactive) {
    interactiveMain();
  } else {
    nonInteractiveMain();
  }
  return 0;
}
