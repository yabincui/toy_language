#ifndef TOY_OPTIMIZATION_H_
#define TOY_OPTIMIZATION_H_

#include <llvm/IR/Module.h>

// Used in interactive mode.
void prepareOptPipeline();
void optPipeline(llvm::Module* Module);
void finishOptPipeline();

// Used in non-interactive mode.
void optMain(llvm::Module* Module);

#endif  // TOY_OPTIMIZATION_H_
