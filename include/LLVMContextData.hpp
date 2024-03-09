#ifndef _LLVM_CONTEXT_DATA_HPP_
#define _LLVM_CONTEXT_DATA_HPP_

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include <map>
#include <string_view>

struct LLVMContextData {
    LLVMContextData(std::string_view moduleName)
        : m_llvmContext()
        , m_builder(m_llvmContext)
        , m_llvmModule(moduleName, m_llvmContext)
        , m_namedValues() {
    }

    LLVMContextData(const LLVMContextData&) = delete;
    LLVMContextData& operator=(const LLVMContextData&) = delete;
    LLVMContextData(LLVMContextData&&) = delete;
    LLVMContextData& operator=(LLVMContextData&&) = delete;
    ~LLVMContextData() = default;

    llvm::LLVMContext m_llvmContext;
    llvm::IRBuilder<> m_builder;
    llvm::Module m_llvmModule;

    std::map<std::string_view, llvm::Value*> m_namedValues;
};

#endif  // !_LLVM_CONTEXT_DATA_HPP_
