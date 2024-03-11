#ifndef _EXPRESSIONS_AST_HPP_
#define _EXPRESSIONS_AST_HPP_

#include <memory>
#include <string>
#include <vector>

#include "Utils.hpp"
#include "LLVMUtils.hpp"
#include "LLVMContextData.hpp"

#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Verifier.h"

class ExprAST {
public:
    ExprAST() = default;
    ExprAST(const ExprAST &) = delete;
    ExprAST &operator=(const ExprAST &) = delete;
    ExprAST(ExprAST &&) = delete;
    ExprAST &operator=(ExprAST &&) = delete;
    virtual ~ExprAST() = default;

    virtual llvm::Value *codegen(LLVMContextData &ctxData) = 0;
};

class NumberExprAST : public ExprAST {
public:
    NumberExprAST(double number)
        : m_value(number) {
    }

    llvm::Value *codegen(LLVMContextData &ctxData) override {
        return llvm::ConstantFP::get(ctxData.m_llvmContext,
                                     llvm::APFloat{m_value});
    }

private:
    double m_value;
};

class VariableExprAST : public ExprAST {
public:
    VariableExprAST(std::string identifier)
        : m_identifier(std::move(identifier)) {
    }

    llvm::Value *codegen(LLVMContextData &ctxData) override {
        llvm::Value *varValue = std::invoke([this, &ctxData] {
            const auto it = ctxData.m_namedValues.find(m_identifier);
            return it != ctxData.m_namedValues.end() ? it->second : nullptr;
        });

        if (!varValue) {
            utils::logErrorLLVMValue("Unknown variable name");
        }

        return varValue;
    }

private:
    std::string m_identifier;
};

class BinaryExprAST : public ExprAST {
public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs,
                  std::unique_ptr<ExprAST> rhs)
        : m_op(op)
        , m_lhs(std::move(lhs))
        , m_rhs(std::move(rhs)) {
    }

    llvm::Value *codegen(LLVMContextData &ctxData) override {
        llvm::Value *lhsValue = m_lhs ? m_lhs->codegen(ctxData) : nullptr;
        if (!lhsValue) {
            return utils::logErrorLLVMValue(
                "Unexpected nullptr value for LHS sub-expression");
        }

        llvm::Value *rhsValue = m_rhs ? m_rhs->codegen(ctxData) : nullptr;
        if (!rhsValue) {
            return utils::logErrorLLVMValue(
                "Unexpected nullptr value for RHS sub-expression");
        }

        switch (m_op) {
            case '+':
                return ctxData.m_builder.CreateFAdd(lhsValue, rhsValue,
                                                    "addtmp");
            case '-':
                return ctxData.m_builder.CreateFSub(lhsValue, rhsValue,
                                                    "subtmp");
            case '*':
                return ctxData.m_builder.CreateFMul(lhsValue, rhsValue,
                                                    "multmp");
            case '<':
                lhsValue = ctxData.m_builder.CreateFCmpULT(lhsValue, rhsValue,
                                                           "cmptmp");
                // Convert bool 0/1 to double 0.0 or 1.0
                return ctxData.m_builder.CreateUIToFP(
                    lhsValue, llvm::Type::getDoubleTy(ctxData.m_llvmContext),
                    "booltmp");
            default:
                return utils::logErrorLLVMValue("Invalid binary operator");
        }

        return nullptr;
    }

private:
    char m_op;
    std::unique_ptr<ExprAST> m_lhs;
    std::unique_ptr<ExprAST> m_rhs;
};

class CallExprAST : public ExprAST {
public:
    CallExprAST(std::string callee, std::vector<std::unique_ptr<ExprAST>> args)
        : m_callee(std::move(callee))
        , m_args(std::move(args)) {
    }

    llvm::Value *codegen(LLVMContextData &ctxData) override {
        // Look up the name in the global module table
        llvm::Function *calleeFunc = ctxData.m_llvmModule.getFunction(m_callee);
        if (!calleeFunc) {
            return utils::logErrorLLVMValue("Unknown function referenced");
        }

        if (calleeFunc->arg_size() != m_args.size()) {
            return utils::logErrorLLVMValue("Incorrect # arguments passed");
        }

        std::vector<llvm::Value*> argValues;
        argValues.reserve(m_args.size());

        for (const auto &arg : m_args) {
            if (!arg) {
                return utils::logErrorLLVMValue(
                    "Unexpected nullptr expr before LLVM codegen");
            }

            argValues.push_back(arg->codegen(ctxData));

            if (!argValues.back()) {
                return utils::logErrorLLVMValue(
                    "Unexpected nullptr expr after LLVM codegen");
            }
        }

        return ctxData.m_builder.CreateCall(calleeFunc, argValues, "calltmp");
    }

private:
    std::string m_callee;
    std::vector<std::unique_ptr<ExprAST>> m_args;
};

class PrototypeAST {
public:
    PrototypeAST(std::string callee, std::vector<std::string> args)
        : m_callee(std::move(callee))
        , m_args(std::move(args)) {
    }

    const std::string &getName() const {
        return m_callee;
    }

    llvm::Function *codegen(LLVMContextData &ctxData) {
        llvm::Type *const doubleType =
            llvm::Type::getDoubleTy(ctxData.m_llvmContext);

        std::vector<llvm::Type *> types{m_args.size(), doubleType};

        llvm::FunctionType *funcType =
            llvm::FunctionType::get(doubleType, types, false);

        llvm::Function *func =
            llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                   m_callee, ctxData.m_llvmModule);

        // Set names for all arguments
        for (int i = 0; llvm::Argument &arg : func->args()) {
            arg.setName(m_args[i++]);
        }

        return func;
    }

private:
    std::string m_callee;
    std::vector<std::string> m_args;
};

class FunctionAST {
public:
    FunctionAST(PrototypeAST prototype, std::unique_ptr<ExprAST> body)
        : m_prototype(std::move(prototype))
        , m_body(std::move(body)) {
    }

    llvm::Function *codegen(LLVMContextData &ctxData) {
        // First, check for an existing function from a previous 'extern'
        // declaration
        llvm::Function *func =
            ctxData.m_llvmModule.getFunction(m_prototype.getName());

        // Create the prototype IR if it wasn't found
        if (!func) {
            func = m_prototype.codegen(ctxData);
        } else {
            // TODO: Verofy that the found prototype signature(args count + arg
            // identifiers match those) is equal to the signature of the current
            // function
            // Possible solution: Cache not only the name of the prototype, but
            // its whole signature(like name mangling)
        }

        if (!func) {
            return utils::logErrorLLVMFunction(
                "codegen() failed to create llvm::Function");
        }

        if (!func->empty()) {
            return utils::logErrorLLVMFunction(
                "codegen() function cannot be redefined");
        }

        // Create a new basic block to start insertion into
        llvm::BasicBlock *basicBlock =
            llvm::BasicBlock::Create(ctxData.m_llvmContext, "entry", func);

        ctxData.m_builder.SetInsertPoint(basicBlock);

        // Record the function arguments in the NamedValues map
        ctxData.m_namedValues.clear();

        for (llvm::Argument &arg : func->args()) {
            ctxData.m_namedValues[arg.getName()] =
                &arg;  // llvm::Argument inherits from llvm::Value
        }

        if (llvm::Value *retValue = m_body->codegen(ctxData); retValue) {
            // Finish off the function
            ctxData.m_builder.CreateRet(retValue);

            // Validate the generated code, checking for consistency
            if (llvm::verifyFunction(*func)) {
                return utils::logErrorLLVMFunction(
                    "codegen() verifyFunction failed");
            }

            auto &JIT = ctxData.m_JIT;
            JIT.m_FPM.run(*func, JIT.m_FAM);

            return func;
        }

        // Error reading body, remove function
        func->eraseFromParent();
        return utils::logErrorLLVMFunction("codegen() of function body failed");
    }

    PrototypeAST m_prototype;
    std::unique_ptr<ExprAST> m_body;
};

#endif  // !_EXPRESSIONS_AST_HPP_
