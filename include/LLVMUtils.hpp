#ifndef _LLVM_UTILS_HPP_
#define _LLVM_UTILS_HPP_

#include "Utils.hpp"

#include "llvm/IR/Value.h"

namespace utils {
inline llvm::Value *logErrorLLVMValue(const char *str) {
    logError(str);
    return nullptr;
}

inline llvm::Function *logErrorLLVMFunction(const char *str) {
    logError(str);
    return nullptr;
}
}  // namespace utils

#endif  // !_LLVM_UTILS_HPP_
