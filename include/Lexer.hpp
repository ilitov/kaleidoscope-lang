#ifndef _LEXER_HPP_
#define _LEXER_HPP_

#include <charconv>
#include <string>
#include <variant>

enum class Token {
    TOK_EOF,
    TOK_DEF,
    TOK_EXTERN,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_OPERATOR
};

using TokenValue = std::variant<char, std::string, double>;

struct TokenData {
    Token m_token;
    TokenValue m_value;
};

class Lexer {
    struct LastCharSaver {
        char &m_lastChar;
        Lexer &m_lexer;
        LastCharSaver(Lexer &lexer, char &lastChar)
            : m_lexer(lexer)
            , m_lastChar(lastChar) {
        }
        ~LastCharSaver() {
            m_lexer.m_lastChar = m_lastChar;
        }
    };

    friend class LastCharSaver;

public:
    TokenData getNextToken() {
        char currentChar = skipSpaces(m_lastChar);
        LastCharSaver lastCharSaver{*this, currentChar};

        // Handle identifiers
        if (beginsIdentifier(currentChar)) {
            std::string identifier;

            while (partOfIdentifier(currentChar)) {
                identifier.push_back(currentChar);
                currentChar = std::getchar();
            }

            if (identifier == "def") {
                return TokenData{Token::TOK_DEF, identifier};
            }
            if (identifier == "extern") {
                return TokenData{Token::TOK_EXTERN, identifier};
            }

            return TokenData{Token::TOK_IDENTIFIER, identifier};
        }

        // Handle numbers
        if (beginsNumber(currentChar)) {
            const double number = std::invoke([&currentChar] {
                std::string number;
                int dotsCount = 0;

                while (beginsNumber(currentChar)) {
                    dotsCount += (currentChar == '.');

                    if (dotsCount > 1) {
                        break;
                    }
                    number.push_back(currentChar);
                    currentChar = std::getchar();
                }

                double numberVal{};
                if (const auto fcr = std::from_chars(
                        number.c_str(), number.c_str() + number.size(),
                        numberVal);
                    fcr.ec != std::error_code{}) {
                    numberVal = std::numeric_limits<double>::quiet_NaN();
                }
                return numberVal;
            });

            return TokenData{Token::TOK_NUMBER, number};
        }

        // Handle comments
        if (currentChar == '#') {
            currentChar = skipLine(currentChar);

            // Recursively get the next token if there is still more data in the
            // input stream
            if (currentChar != EOF) {
                return getNextToken();
            }
        }

        if (currentChar == EOF) {
            return TokenData{Token::TOK_EOF, static_cast<char>(EOF)};
        }

        // Handle operator
        const char op = currentChar;
        currentChar = std::getchar();

        return TokenData{Token::TOK_OPERATOR, op};
    }

private:
    static char skipSpaces(char c) {
        while (std::isspace(c)) {
            c = std::getchar();
        }
        return c;
    }

    static char skipLine(char c) {
        while (c != EOF && c != '\r' && c != '\n') {
            c = std::getchar();
        }
        return c;
    }

    static bool beginsIdentifier(char c) {
        return c == '_' || std::isalpha(c);
    }

    static bool partOfIdentifier(char c) {
        return c == '_' || std::isalnum(c);
    }

    static bool beginsNumber(char c) {
        return c == '.' || std::isdigit(c);
    }

private:
    char m_lastChar = ' ';
};

#endif  // !_LEXER_HPP_
