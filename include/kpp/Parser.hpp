
#pragma once

#include "AST.hpp"
#include "Token.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace kpp {

class Parser
{
public:

    explicit Parser(
        const std::vector<Token>& tokens);

    std::unique_ptr<Program> parse();

private:

    // ========================================================
    // TOKENS
    // ========================================================

    const Token& current() const;

    bool check(TokenType type) const;

    const Token& advance();

    bool match(TokenType type);

    void expect(
        TokenType type,
        const char* message);


    // ========================================================
    // PROGRAMA
    // ========================================================

    std::unique_ptr<FunctionDeclaration>
    parseFunction();


    // ========================================================
    // BLOQUE
    // ========================================================

    std::unique_ptr<Block>
    parseBlock();


    // ========================================================
    // SENTENCIAS
    // ========================================================

    std::unique_ptr<Statement>
    parseStatement();


    // ========================================================
    // EXPRESIONES
    // ========================================================

    // Entrada principal de expresiones.
    std::unique_ptr<Expression>
    parseExpression();

    // --------------------------------------------------------
    // LOGICA
    // --------------------------------------------------------

    std::unique_ptr<Expression>
    parseLogicalOr();

    std::unique_ptr<Expression>
    parseLogicalAnd();

    // --------------------------------------------------------
    // COMPARACION
    // --------------------------------------------------------

    std::unique_ptr<Expression>
    parseEquality();

    std::unique_ptr<Expression>
    parseComparison();

    // --------------------------------------------------------
    // ARITMETICA
    // --------------------------------------------------------

    std::unique_ptr<Expression>
    parseTerm();

    std::unique_ptr<Expression>
    parseFactor();

    std::unique_ptr<Expression>
    parsePower();

    // --------------------------------------------------------
    // UNARIOS / PRIMARIOS
    // --------------------------------------------------------

    std::unique_ptr<Expression>
    parseUnary();

    std::unique_ptr<Expression>
    parsePrimary();


    // ========================================================
    // DATOS DEL PARSER
    // ========================================================

    const std::vector<Token>& tokens;

    std::size_t position = 0;
};

}
