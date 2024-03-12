#ifndef _LLVM_CONTEXT_DATA_HPP_
#define _LLVM_CONTEXT_DATA_HPP_

// LLVMContextData includes
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

// LLVMOptContextData includes
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/CGSCCPassManager.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"

#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

#include <map>
#include <string_view>
#include <memory>

struct LLVMOptContextData {
    LLVMOptContextData(llvm::LLVMContext& llvmCtx);
    LLVMOptContextData(const LLVMOptContextData&) = delete;
    LLVMOptContextData& operator=(const LLVMOptContextData&) = delete;
    LLVMOptContextData(LLVMOptContextData&&) = delete;
    LLVMOptContextData& operator=(LLVMOptContextData&&) = delete;
    ~LLVMOptContextData() = default;

    // Pass and analysis managers
    llvm::FunctionPassManager m_FPM;
    llvm::LoopAnalysisManager m_LAM;
    llvm::FunctionAnalysisManager m_FAM;
    llvm::CGSCCAnalysisManager m_CGAM;
    llvm::ModuleAnalysisManager m_MAM;
    llvm::PassInstrumentationCallbacks m_PIC;
    llvm::StandardInstrumentations m_SI;
};

struct LLVMContextData {
    LLVMContextData(std::string_view moduleName);
    LLVMContextData(const LLVMContextData&) = delete;
    LLVMContextData& operator=(const LLVMContextData&) = delete;
    LLVMContextData(LLVMContextData&&) = delete;
    LLVMContextData& operator=(LLVMContextData&&) = delete;
    ~LLVMContextData() = default;

    std::unique_ptr<llvm::LLVMContext> m_llvmContext;
    llvm::IRBuilder<> m_builder;
    std::unique_ptr<llvm::Module> m_llvmModule;

    std::map<std::string_view, llvm::Value*> m_namedValues;

    LLVMOptContextData m_llvmOpt;
};

#endif  // !_LLVM_CONTEXT_DATA_HPP_
