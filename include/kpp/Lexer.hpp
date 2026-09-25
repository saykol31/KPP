#pragma once

#include "Token.hpp"

#include <string>
#include <vector>

namespace kpp {

class Lexer
{
public:

    explicit Lexer(const std::string& source);

    std::vector<Token> tokenize();

private:

    char peek() const;
    char advance();

    void skipWhitespace();

    Token identifier();
    Token number();
    Token stringLiteral();

    std::string source;

    std::size_t position = 0;

    int line = 1;
    int column = 1;
};

}
