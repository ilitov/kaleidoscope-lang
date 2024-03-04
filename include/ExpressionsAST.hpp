#ifndef _EXPRESSIONS_AST_HPP_
#define _EXPRESSIONS_AST_HPP_

#include <memory>
#include <string>
#include <vector>

class ExprAST {
public:
    ExprAST() = default;
    ExprAST(const ExprAST &) = delete;
    ExprAST &operator=(const ExprAST &) = delete;
    ExprAST(ExprAST &&) = delete;
    ExprAST &operator=(ExprAST &&) = delete;
    virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST {
public:
    NumberExprAST(double number)
        : m_value(number) {
    }

private:
    double m_value;
};

class VariableExprAST : public ExprAST {
public:
    VariableExprAST(std::string identifier)
        : m_identifier(std::move(identifier)) {
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

private:
    char m_op;
    std::unique_ptr<ExprAST> m_lhs;
    std::unique_ptr<ExprAST> m_rhs;
};

class CallExprAST : public ExprAST {
public:
    template <typename... Args>
    CallExprAST(std::string callee, std::unique_ptr<Args>... args)
        : m_callee(std::move(callee))
        , m_args() {
        (m_args.push_back(std::move(args)), ...);
    }

private:
    std::string m_callee;
    std::vector<std::unique_ptr<ExprAST>> m_args;
};

class PrototypeAST {
public:
    template <typename... Args>
    PrototypeAST(std::string callee, Args... args)
        : m_callee(std::move(callee))
        , m_args() {
        (m_args.push_back(std::move(args)), ...);
    }

    const std::string &getName() const {
        return m_callee;
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

    PrototypeAST m_prototype;
    std::unique_ptr<ExprAST> m_body;
};

#endif  // !_EXPRESSIONS_AST_HPP_
