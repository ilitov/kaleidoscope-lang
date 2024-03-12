#ifndef _PARSER_H_
#define _PARSER_H_

#include "ExpressionsAST.hpp"
#include "Lexer.hpp"
#include "LLVMContextData.hpp"

#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"

#include "KaleidoscopeJIT.h"

#include <cassert>
#include <map>

class Parser {
    static const inline std::map<char, int> BinOpPrecedence{
        {'<', 10}, {'+', 20}, {'-', 20}, {'*', 40}};

    static const char EOS = ';'; // end of statement

    static constexpr const char *kModuleName = "Kaleidoscope goes jiitttt";
    static constexpr const char *kAnonExprIdentifier = "__anon_expr";

    static std::unique_ptr<LLVMContextData> initializeLLVMContextData(std::string_view moduleName,
                                                                                    const llvm::orc::KaleidoscopeJIT &JIT) {
        auto llvmCtxData = std::make_unique<LLVMContextData>(moduleName);
        llvmCtxData->m_llvmModule->setDataLayout(JIT.getDataLayout());
        return llvmCtxData;
    }

    void updateLLVMContextData(std::string_view moduleName = kModuleName) {
        m_llvmCtxData = initializeLLVMContextData(kModuleName, *m_JIT);
    }

public:
    Parser(Lexer &lexer)
        : m_lexer(lexer)
        , m_llvmCtxData()
        , m_JIT() {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmParser();
        llvm::InitializeNativeTargetAsmPrinter();

        llvm::ExitOnError{}(llvm::orc::KaleidoscopeJIT::Create().moveInto(m_JIT));
        m_llvmCtxData = initializeLLVMContextData(kModuleName, *m_JIT);
    }

    ~Parser() {
        // Print everything on exit
        m_llvmCtxData->m_llvmModule->print(llvm::outs(), nullptr);
    }

    /// top ::= definition | external | expression | ';'
    void mainLoop() {
        const auto printPrompt = [] { std::cout << "ready> "; };

        printPrompt();

        // Read the first token
        advanceCurrentToken();

        while (true) {
            switch (m_currentToken.m_token) {
                case Token::TOK_EOF:
                    std::cout << "EOF\n";
                    return;
                case Token::TOK_DEF:
                    handleDefinition();
                    break;
                case Token::TOK_EXTERN:
                    handleExtern();
                    break;
                case Token::TOK_OPERATOR:
                    // Ignore top-level semicolons
                    if (isOperator(m_currentToken, EOS)) {
                        printPrompt();
                        advanceCurrentToken();
                        break;
                    }
                    // fallthrough to top-level expression for operators that are not ';'
                default:
                    handleTopLevelExpression();
                    break;
            }
        }
    }

private:
    void handleDefinition() {
        if (const auto funcDef = parseDefinition(); funcDef) {
            std::cout << "Parsed a function definition\n";

            if (const auto value = funcDef->codegen(*m_llvmCtxData); value) {
                value->print(llvm::outs());
                std::cout << '\n';
            }
        }
    }

    void handleExtern() {
        if (const auto externProto = parseExtern(); externProto) {
            std::cout << "Parsed an extern\n";

            if (const auto value = externProto->codegen(*m_llvmCtxData); value) {
                value->print(llvm::outs());
                std::cout << '\n';
            }
        }
    }

    void handleTopLevelExpression() {
        if (const auto anonFunc = parseTopLevelExpr(); anonFunc) {
            std::cout << "Parsed a top-level expr\n";

            if (const auto value = anonFunc->codegen(*m_llvmCtxData); value) {
                value->print(llvm::outs());
                std::cout << '\n';

                // Remove anon function from the LLVM Module(unlink + delete)
                // value->eraseFromParent();

                // Create a ResourceTracker to track JIT'd memory allocated to our anonymous
                // expression -- that way we can free it after executing
                auto resourceTracker = m_JIT->getMainJITDylib().createResourceTracker();

                auto threadSafeModule =
                    llvm::orc::ThreadSafeModule(std::move(m_llvmCtxData->m_llvmModule), std::move(m_llvmCtxData->m_llvmContext));

                llvm::ExitOnError{}(m_JIT->addModule(std::move(threadSafeModule), resourceTracker));

                // We lost the module -> create new one(just build the whole LLVM context with all passes and managers)
                updateLLVMContextData();

                // Search the JIT for the __anon_expr symbol
                auto exprSymbol = llvm::ExitOnError{}(m_JIT->lookup(kAnonExprIdentifier));

                // Get the symbol's address and cast it to the right type (takes no arguments, returns a double) so we can call
                // it as a native function
                double (*nativeAnonFunc)() = exprSymbol.getAddress().toPtr<double (*)()>();
                std::cout << "Evaluated to " << nativeAnonFunc() << '\n';

                // Delete the anonymous expression module from the JIT
                llvm::ExitOnError{}(resourceTracker->remove());
            }
        }
    }

private:
    /// primary
    ///   ::= identifierexpr
    ///   ::= numberexpr
    ///   ::= parenexpr
    std::unique_ptr<ExprAST> parsePrimary() {
        switch (m_currentToken.m_token) {
            case Token::TOK_IDENTIFIER:
                return parseIdentifierExpr(
                    std::get<std::string>(std::move(m_currentToken.m_value)));
            case Token::TOK_NUMBER:
                return parseNumberExpr(
                    std::get<double>(std::move(m_currentToken.m_value)));
            case Token::TOK_OPERATOR:
                if (openParen(m_currentToken)) {
                    return parseParenExpr();
                }
        }

        if (!isOperator(m_currentToken, EOS)) {
            // eat unknown primary expr
            advanceCurrentToken();
        }

        return utils::logError("unknown token when expecting an expression");
    }

    /// binoprhs
    ///   ::= ('+' primary)*
    std::unique_ptr<ExprAST> parseBinOpRhs(int minExprPrec,
                                           std::unique_ptr<ExprAST> lhs) {
        // If this is a binop, find its precedence.
        while (true) {
            const int tokPrec = getTokenPrecedence(m_currentToken);

            // If this is a binop that binds at least as tightly as the current
            // binop, consume it, otherwise we are done.
            if (tokPrec < minExprPrec) {
                return lhs;
            }

            const auto binOp = m_currentToken;

            // eat current binop
            advanceCurrentToken();

            auto rhs = parsePrimary();
            if (!rhs) {
                return nullptr;
            }

            const int nextTokPrec = getTokenPrecedence(m_currentToken);

            // If BinOp binds less tightly with RHS than the operator after RHS,
            // let the pending operator take RHS as its LHS.
            if (tokPrec < nextTokPrec) {
                rhs = parseBinOpRhs(minExprPrec + 1, std::move(rhs));
                if (!rhs) {
                    return nullptr;
                }
            }

            assert(binOp.m_token == Token::TOK_OPERATOR &&
                   "First BinOp in sequence is not a binary operator");

            lhs = std::make_unique<BinaryExprAST>(
                std::get<char>(binOp.m_value), std::move(lhs), std::move(rhs));
        }

        return nullptr;
    }

    /// expression
    ///   ::= primary binoprhs
    std::unique_ptr<ExprAST> parseExpression() {
        auto lhs = parsePrimary();
        if (!lhs) {
            return nullptr;
        }

        return parseBinOpRhs(0, std::move(lhs));
    }

    /// prototype
    ///   ::= id '(' id* ')'
    std::unique_ptr<PrototypeAST> parsePrototype() {
        if (m_currentToken.m_token != Token::TOK_IDENTIFIER) {
            return utils::logErrorProto("Expected function name in prototype");
        }

        std::string fnName = std::get<std::string>(m_currentToken.m_value);

        // eat function identifier
        advanceCurrentToken();

        if (!openParen(m_currentToken)) {
            return utils::logErrorProto("Expected '(' in prototype");
        }

        // eat (
        advanceCurrentToken();

        // Read the list of argument names
        std::vector<std::string> args;

        while (true) {
            if (m_currentToken.m_token != Token::TOK_IDENTIFIER) {
                break;
            }

            args.push_back(std::get<std::string>(m_currentToken.m_value));

            // eat arg identifier
            advanceCurrentToken();
        }

        if (!closeParen(m_currentToken)) {
            return utils::logErrorProto("Expected ')' in prototype");
        }

        // eat )
        advanceCurrentToken();

        return std::make_unique<PrototypeAST>(std::move(fnName),
                                              std::move(args));
    }

    /// definition ::= 'def' prototype expression
    std::unique_ptr<FunctionAST> parseDefinition() {
        // eat def
        advanceCurrentToken();

        auto proto = parsePrototype();
        if (!proto) {
            return nullptr;
        }

        auto expr = parseExpression();
        if (!expr) {
            return nullptr;
        }

        return std::make_unique<FunctionAST>(std::move(*proto),
                                             std::move(expr));
    }

    /// external ::= 'extern' prototype
    std::unique_ptr<PrototypeAST> parseExtern() {
        // eat extern
        advanceCurrentToken();
        return parsePrototype();
    }

    /// toplevelexpr ::= expression
    std::unique_ptr<FunctionAST> parseTopLevelExpr() {
        auto expr = parseExpression();
        if (!expr) {
            return nullptr;
        }

        // Make an anonymous proto
        auto proto =
            std::make_unique<PrototypeAST>(kAnonExprIdentifier, std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(*proto),
                                             std::move(expr));
    }

    /// numberexpr ::= number
    std::unique_ptr<ExprAST> parseNumberExpr(double value) {
        auto result = std::make_unique<NumberExprAST>(value);

        // eat number
        advanceCurrentToken();

        return result;
    }

    /// identifierexpr
    ///   ::= identifier
    ///   ::= identifier '(' expression* ')'
    std::unique_ptr<ExprAST> parseIdentifierExpr(std::string value) {
        // eat identifier
        advanceCurrentToken();

        // Handle variable
        if (!openParen(m_currentToken)) {
            return std::make_unique<VariableExprAST>(std::move(value));
        }

        // eat (
        advanceCurrentToken();

        // Handle function call
        std::vector<std::unique_ptr<ExprAST>> args;

        while (true) {
            if (closeParen(m_currentToken)) {
                break;
            }

            auto arg = parseExpression();
            if (!arg) {
                return nullptr;
            }

            args.push_back(std::move(arg));

            if (!comma(m_currentToken)) {
                if (closeParen(m_currentToken)) {
                    break;
                }

                return utils::logError("Expected ')' or ',' in argument list");
            }

            // eat comma
            advanceCurrentToken();
        }

        // eat )
        advanceCurrentToken();

        return std::make_unique<CallExprAST>(std::move(value), std::move(args));
    }

    /// parenexpr ::= '(' expression ')'
    std::unique_ptr<ExprAST> parseParenExpr() {
        // eat (
        advanceCurrentToken();

        auto expr = parseExpression();
        if (!expr) {
            return utils::logError("null expression in parseParenExpr()");
        }

        if (!closeParen(m_currentToken)) {
            return utils::logError("expected ')'");
        }

        // eat )
        advanceCurrentToken();

        return expr;
    }

    void advanceCurrentToken() {
        m_currentToken = m_lexer.getNextToken();
    }

    static bool isOperator(const TokenData &td, char op) {
        return td.m_token == Token::TOK_OPERATOR &&
               std::get<char>(td.m_value) == op;
    }

    static bool openParen(const TokenData &td) {
        return isOperator(td, '(');
    }

    static bool closeParen(const TokenData &td) {
        return isOperator(td, ')');
    }

    static bool comma(const TokenData &td) {
        return isOperator(td, ',');
    }

    static int getTokenPrecedence(const TokenData &td) {
        const char c = std::invoke([&td] {
            if (td.m_token == Token::TOK_OPERATOR) {
                return std::get<char>(td.m_value);
            }
            return '\0';
        });

        const auto it = BinOpPrecedence.find(c);
        return it != BinOpPrecedence.end() ? it->second : -1;
    }

private:
    Lexer &m_lexer;
    TokenData m_currentToken;

    std::unique_ptr<LLVMContextData> m_llvmCtxData;

    std::unique_ptr<llvm::orc::KaleidoscopeJIT> m_JIT;
};

#endif  // !_PARSER_H_
