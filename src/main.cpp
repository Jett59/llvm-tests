#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>

// Include for Global.
#include <llvm/IR/GlobalVariable.h>

using namespace llvm;

int main() {
  LLVMContext context;
  Module module("test", context);
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  InitializeNativeTargetDisassembler();
  std::string targetTriple = sys::getDefaultTargetTriple();
  module.setTargetTriple(targetTriple);
  std::string error;
  auto target = TargetRegistry::lookupTarget(targetTriple, error);
  if (!target) {
    errs() << error;
    return 1;
  }
  auto cpu = "generic";
  auto features = "";
  TargetOptions opt;
  auto rm = Optional<Reloc::Model>();
  auto targetMachine =
      target->createTargetMachine(targetTriple, cpu, features, opt, rm);
  module.setDataLayout(targetMachine->createDataLayout());

  Function *function =
      Function::Create(FunctionType::get(Type::getInt32Ty(context), false),
                       GlobalValue::ExternalLinkage, "main", module);
  BasicBlock *entryBlock = BasicBlock::Create(context, "entry", function);
  IRBuilder<> builder(entryBlock);
  GlobalVariable *global = new GlobalVariable(
      module, Type::getFloatTy(context), false, GlobalValue::ExternalLinkage,
      ConstantFP::get(Type::getFloatTy(context), 3.14), "global");
  Value *loadInst =
      builder.CreateLoad(Type::getFloatTy(context), global, "load");
  // Add one and store back to global to ensure the load is done as float.
  Value *addInst = builder.CreateFAdd(
      loadInst, ConstantFP::get(Type::getFloatTy(context), 1.0), "add");
  builder.CreateStore(addInst, global);
  // Now bitcast the loaded value to i32 and return it.
  Value *bitcastInst =
      builder.CreateBitCast(loadInst, Type::getInt32Ty(context), "bitcast");
  builder.CreateRet(bitcastInst);
  // Verify and print the module.
  module.print(errs(), nullptr);
  if (verifyModule(module, &errs())) {
    errs() << "Error verifying module\n";
    return 1;
  }
  // Create the assembly code.
  std::error_code ec;
  raw_fd_ostream dest("test.s", ec, sys::fs::OF_None);
  if (ec) {
    errs() << "Could not open file: " << ec.message();
    return 1;
  }
  legacy::PassManager pass;
  auto fileType = CGFT_AssemblyFile;
  if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
    errs() << "TargetMachine can't emit a file of this type";
    return 1;
  }
  pass.run(module);
  dest.flush();
}
