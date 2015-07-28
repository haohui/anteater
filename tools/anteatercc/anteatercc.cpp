#include "anteater/Target/Backend.h"

#include <llvm/Transforms/Scalar.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/IRReader.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SystemUtils.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>

using namespace llvm;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
    cl::init("-"), cl::value_desc("filename"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Override output filename"),
               cl::value_desc("filename"), cl::init("-"));

static cl::opt<std::string>
Backend("backend", cl::desc("backend"),
               cl::value_desc("Yices/SMT12"), cl::init("yices"));

int main(int argc, char * argv[]) {
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  LLVMContext &Context = getGlobalContext();
  
  cl::ParseCommandLineOptions(argc, argv, "Anteater compiler\n");

  SMDiagnostic Err;

  // Load the input module...
  std::auto_ptr<Module> M;
  M.reset(ParseIRFile(InputFilename, Err, Context));

  if (M.get() == 0) {
    Err.Print(argv[0], errs());
    return 1;
  }

  // Figure out what stream we are supposed to write to...
  // FIXME: outs() is not binary!
  raw_ostream *Out = &outs();  // Default to printing to stdout...
  if (OutputFilename != "-") {
    std::string ErrorInfo;
    Out = new raw_fd_ostream(OutputFilename.c_str(), ErrorInfo,
                               raw_fd_ostream::F_Binary);
    if (!ErrorInfo.empty()) {
      errs() << ErrorInfo << '\n';
      delete Out;
      return 1;
    }
  }

  PassManager Passes;
  if (Backend == "yices") {
    Passes.add(anteater::createXorEliminationPass(&Context));
    Passes.add(anteater::createAnteaterInstructionNamerPass());
    Passes.add(anteater::createYicesWriter(Out));
    
  } else {
    Passes.add(anteater::createAnteaterInstructionNamerPass());
    Passes.add(anteater::createSMT12Writer(Out));    
  }
  Passes.run(*M.get());


  // Delete the raw_fd_ostream.
  if (Out != &outs())
    delete Out;
  
  return 0;
}
