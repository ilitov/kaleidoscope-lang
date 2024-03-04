#include <iostream>

#include "Parser.hpp"

constexpr bool TEST_LEXER = false;
constexpr bool TEST_PARSER = true;

int main() {
    if constexpr (TEST_LEXER) {
        Lexer lexer;

        for (TokenData td = lexer.getNextToken();; td = lexer.getNextToken()) {
            const auto unpackValue = [](const auto &value) {
                return std::visit(
                    []<typename T>(const T &val) {
                        if constexpr (std::is_same_v<T, char>) {
                            return std::string{val};
                        } else if constexpr (std::is_same_v<T, std::string>) {
                            return val;
                        } else {
                            return std::to_string(val);
                        }
                    },
                    value);
            };

            switch (td.m_token) {
                case Token::TOK_EOF:
                    std::cout << "EOF\n";
                    break;
                case Token::TOK_DEF:
                    std::cout << "def\n";
                    break;
                case Token::TOK_EXTERN:
                    std::cout << "extern\n";
                    break;
                case Token::TOK_IDENTIFIER:
                    std::cout << "id: " << unpackValue(td.m_value) << '\n';
                    break;
                case Token::TOK_NUMBER:
                    std::cout << "num: " << unpackValue(td.m_value) << '\n';
                    break;
                case Token::TOK_OPERATOR:
                    std::cout << "op: '" << unpackValue(td.m_value) << "'\n";
                    break;
            }
        }
    }

    if constexpr (TEST_PARSER) {
        Lexer lexer;
        Parser parser{lexer};

        parser.mainLoop();
    }

    return 0;
}
