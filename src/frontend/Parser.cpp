
#include "kpp/Parser.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace kpp {

// ============================================================
// CONSTRUCTOR
// ============================================================

Parser::Parser(
    const std::vector<Token>& tokens)
    : tokens(tokens)
{
}


// ============================================================
// CURRENT
// ============================================================

const Token& Parser::current() const
{
    if (position >= tokens.size())
        return tokens.back();

    return tokens[position];
}


// ============================================================
// CHECK
// ============================================================

bool Parser::check(TokenType type) const
{
    return current().type == type;
}


// ============================================================
// ADVANCE
// ============================================================

const Token& Parser::advance()
{
    const Token& token = current();

    if (position < tokens.size())
        ++position;

    return token;
}


// ============================================================
// MATCH
// ============================================================

bool Parser::match(TokenType type)
{
    if (!check(type))
        return false;

    advance();

    return true;
}


// ============================================================
// EXPECT
// ============================================================

void Parser::expect(
    TokenType type,
    const char* message)
{
    if (!check(type))
    {
        std::cerr
            << "K++ Parser error at "
            << current().line
            << ":"
            << current().column
            << ": "
            << message
            << '\n';

        throw std::runtime_error(
            "K++ parser error");
    }

    advance();
}


// ============================================================
// PARSE
// ============================================================

std::unique_ptr<Program>
Parser::parse()
{
    auto program =
        std::make_unique<Program>();

    while (!check(TokenType::EndOfFile))
    {
        program->functions.push_back(
            parseFunction());
    }

    return program;
}


// ============================================================
// PARSE FUNCTION
// ============================================================

std::unique_ptr<FunctionDeclaration>
Parser::parseFunction()
{
    // --------------------------------------------------------
    // int
    // --------------------------------------------------------

    expect(
        TokenType::KwInt,
        "se esperaba 'int'");


    // --------------------------------------------------------
    // nombre de función
    // --------------------------------------------------------

    if (!check(TokenType::Identifier))
    {
        throw std::runtime_error(
            "K++: se esperaba nombre de función");
    }

    std::string name =
        advance().text;


    // --------------------------------------------------------
    // (
    // --------------------------------------------------------

    expect(
        TokenType::LParen,
        "se esperaba '('");


    // --------------------------------------------------------
    // )
    // --------------------------------------------------------

    expect(
        TokenType::RParen,
        "se esperaba ')'");


    // --------------------------------------------------------
    // cuerpo
    // --------------------------------------------------------

    auto body =
        parseBlock();


    // --------------------------------------------------------
    // crear función
    // --------------------------------------------------------

    auto function =
        std::make_unique<FunctionDeclaration>();

    function->returnType = "int";
    function->name = name;
    function->body = std::move(body);

    return function;
}


// ============================================================
// PARSE BLOCK
// ============================================================

std::unique_ptr<Block>
Parser::parseBlock()
{
    expect(
        TokenType::LBrace,
        "se esperaba '{'");

    auto block =
        std::make_unique<Block>();

    while (
        !check(TokenType::RBrace)
        && !check(TokenType::EndOfFile))
    {
        block->statements.push_back(
            parseStatement());
    }

    expect(
        TokenType::RBrace,
        "se esperaba '}'");

    return block;
}


// ============================================================
// PARSE STATEMENT
// ============================================================

std::unique_ptr<Statement>
Parser::parseStatement()
{
    // ========================================================
    // VARIABLE
    //
    // int x = 42;
    // int x = 10 + 20 * 3;
    // ========================================================

    if (match(TokenType::KwInt))
    {
        // ----------------------------------------------------
        // nombre
        // ----------------------------------------------------

        if (!check(TokenType::Identifier))
        {
            throw std::runtime_error(
                "K++: se esperaba nombre de variable");
        }

        std::string name =
            advance().text;


        // ----------------------------------------------------
        // =
        // ----------------------------------------------------

        expect(
            TokenType::Assign,
            "se esperaba '='");


        // ----------------------------------------------------
        // expresión inicial
        // ----------------------------------------------------

        auto initializer =
            parseExpression();


        // ----------------------------------------------------
        // ;
        // ----------------------------------------------------

        expect(
            TokenType::Semicolon,
            "se esperaba ';'");

        return std::make_unique<VariableDeclaration>(
            name,
            std::move(initializer));
    }


    // ========================================================
    // PRINT INTEGER
    //
    // print_int(x);
    // ========================================================

    if (check(TokenType::Identifier) &&
        current().text == "print_int")
    {
        advance();

        expect(
            TokenType::LParen,
            "se esperaba '(' después de print_int");

        auto expression =
            parseExpression();

        expect(
            TokenType::RParen,
            "se esperaba ')' después de print_int");

        expect(
            TokenType::Semicolon,
            "se esperaba ';' después de print_int");

        return std::make_unique<PrintIntegerStatement>(
            std::move(expression));
    }


    // ========================================================
    // ASSIGNMENT
    //
    // x = x - 1;
    // ========================================================

    if (check(TokenType::Identifier))
    {
        std::string name =
            advance().text;

        expect(
            TokenType::Assign,
            "se esperaba '=' después del nombre de variable");

        auto expression =
            parseExpression();

        expect(
            TokenType::Semicolon,
            "se esperaba ';' después de asignación");

        return std::make_unique<AssignmentStatement>(
            name,
            std::move(expression));
    }


    // ========================================================
    // IF
    //
    // if (x > 5)
    // {
    //     print("IF OK");
    // }
    // ========================================================

    if (match(TokenType::KwIf))
    {
        expect(
            TokenType::LParen,
            "se esperaba '(' después de if");

        auto condition =
            parseExpression();

        expect(
            TokenType::RParen,
            "se esperaba ')' después de condición de if");

        auto thenBlock =
            parseBlock();

        // ----------------------------------------------------
        // ELSE
        // ----------------------------------------------------

        std::unique_ptr<Block> elseBlock = nullptr;

        if (match(TokenType::KwElse))
        {
            // ------------------------------------------------
            // ELSE IF
            // ------------------------------------------------

            if (check(TokenType::KwIf))
            {
                auto nestedIf =
                    parseStatement();

                elseBlock =
                    std::make_unique<Block>();

                elseBlock->statements.push_back(
                    std::move(nestedIf));
            }
            else
            {
                // --------------------------------------------
                // ELSE normal
                // --------------------------------------------

                elseBlock =
                    parseBlock();
            }
        }

        return std::make_unique<IfStatement>(
            std::move(condition),
            std::move(thenBlock),
            std::move(elseBlock));
    }


    // ========================================================
    // WHILE
    //
    // while (x > 0)
    // {
    //     ...
    // }
    // ========================================================

    if (match(TokenType::KwWhile))
    {
        expect(
            TokenType::LParen,
            "se esperaba '(' después de while");

        auto condition =
            parseExpression();

        expect(
            TokenType::RParen,
            "se esperaba ')' después de condición de while");

        auto body =
            parseBlock();

        return std::make_unique<WhileStatement>(
            std::move(condition),
            std::move(body));
    }


    // ========================================================
    // RETURN
    //
    // return 42;
    // return x;
    // return x + y * 5;
    // ========================================================

    if (match(TokenType::KwReturn))
    {
        auto expression =
            parseExpression();

        expect(
            TokenType::Semicolon,
            "se esperaba ';'");

        return std::make_unique<ReturnStatement>(
            std::move(expression));
    }


    // ========================================================
    // PRINT
    //
    // print("Hola");
    // ========================================================

    if (match(TokenType::KwPrint))
    {
        expect(
            TokenType::LParen,
            "se esperaba '(' después de print");

        if (!check(TokenType::String))
        {
            throw std::runtime_error(
                "K++: print() requiere una cadena");
        }

        std::string text =
            advance().text;

        expect(
            TokenType::RParen,
            "se esperaba ')'");

        expect(
            TokenType::Semicolon,
            "se esperaba ';'");

        return std::make_unique<PrintStatement>(
            text);
    }


    // ========================================================
    // ERROR
    // ========================================================

    throw std::runtime_error(
        "K++: sentencia no reconocida");
}


// ============================================================
// PARSE EXPRESSION
//
// Entrada principal.
//
// PRECEDENCIA:
//
// 1.  ||
// 2.  &&
// 3.  == !=
// 4.  < <= > >=
// 5.  + -
// 6.  * / %
// 7.  ^
// 8.  ! -
// 9.  primarios
// ============================================================

std::unique_ptr<Expression>
Parser::parseExpression()
{
    return parseLogicalOr();
}


// ============================================================
// PARSE LOGICAL OR
//
// a || b
// ============================================================

std::unique_ptr<Expression>
Parser::parseLogicalOr()
{
    auto expression =
        parseLogicalAnd();

    while (match(TokenType::LogicalOr))
    {
        auto right =
            parseLogicalAnd();

        expression =
            std::make_unique<BinaryExpression>(
                "||",
                std::move(expression),
                std::move(right));
    }

    return expression;
}


// ============================================================
// PARSE LOGICAL AND
//
// a && b
// ============================================================

std::unique_ptr<Expression>
Parser::parseLogicalAnd()
{
    auto expression =
        parseEquality();

    while (match(TokenType::LogicalAnd))
    {
        auto right =
            parseEquality();

        expression =
            std::make_unique<BinaryExpression>(
                "&&",
                std::move(expression),
                std::move(right));
    }

    return expression;
}


// ============================================================
// PARSE EQUALITY
//
// a == b
// a != b
// ============================================================

std::unique_ptr<Expression>
Parser::parseEquality()
{
    auto expression =
        parseComparison();

    while (true)
    {
        std::string op;

        if (match(TokenType::Equal))
        {
            op = "==";
        }
        else if (match(TokenType::NotEqual))
        {
            op = "!=";
        }
        else
        {
            break;
        }

        auto right =
            parseComparison();

        expression =
            std::make_unique<BinaryExpression>(
                op,
                std::move(expression),
                std::move(right));
    }

    return expression;
}


// ============================================================
// PARSE COMPARISON
//
// a < b
// a <= b
// a > b
// a >= b
// ============================================================

std::unique_ptr<Expression>
Parser::parseComparison()
{
    auto expression =
        parseTerm();

    while (true)
    {
        std::string op;

        if (match(TokenType::Less))
        {
            op = "<";
        }
        else if (match(TokenType::LessEqual))
        {
            op = "<=";
        }
        else if (match(TokenType::Greater))
        {
            op = ">";
        }
        else if (match(TokenType::GreaterEqual))
        {
            op = ">=";
        }
        else
        {
            break;
        }

        auto right =
            parseTerm();

        expression =
            std::make_unique<BinaryExpression>(
                op,
                std::move(expression),
                std::move(right));
    }

    return expression;
}


// ============================================================
// PARSE TERM
//
// a + b
// a - b
// ============================================================

std::unique_ptr<Expression>
Parser::parseTerm()
{
    auto expression =
        parseFactor();

    while (true)
    {
        std::string op;

        if (match(TokenType::Plus))
        {
            op = "+";
        }
        else if (match(TokenType::Minus))
        {
            op = "-";
        }
        else
        {
            break;
        }

        auto right =
            parseFactor();

        expression =
            std::make_unique<BinaryExpression>(
                op,
                std::move(expression),
                std::move(right));
    }

    return expression;
}


// ============================================================
// PARSE FACTOR
//
// a * b
// a / b
// a % b
// ============================================================

std::unique_ptr<Expression>
Parser::parseFactor()
{
    auto expression =
        parsePower();

    while (true)
    {
        std::string op;

        if (match(TokenType::Multiply))
        {
            op = "*";
        }
        else if (match(TokenType::Divide))
        {
            op = "/";
        }
        else if (match(TokenType::Modulo))
        {
            op = "%";
        }
        else
        {
            break;
        }

        auto right =
            parsePower();

        expression =
            std::make_unique<BinaryExpression>(
                op,
                std::move(expression),
                std::move(right));
    }

    return expression;
}


// ============================================================
// PARSE POWER
//
// a ^ b
//
// La potencia se mantiene separada para poder darle
// posteriormente asociatividad propia.
// ============================================================

std::unique_ptr<Expression>
Parser::parsePower()
{
    auto expression =
        parseUnary();

    while (match(TokenType::Power))
    {
        auto right =
            parseUnary();

        expression =
            std::make_unique<BinaryExpression>(
                "^",
                std::move(expression),
                std::move(right));
    }

    return expression;
}


// ============================================================
// PARSE UNARY
//
// !a
// -a
// ============================================================

std::unique_ptr<Expression>
Parser::parseUnary()
{
    // --------------------------------------------------------
    // LOGICAL NOT
    // --------------------------------------------------------

    if (match(TokenType::LogicalNot))
    {
        auto right =
            parseUnary();

        // ----------------------------------------------------
        // El AST actual solamente posee BinaryExpression.
        //
        // Representamos temporalmente el operador unario
        // como:
        //
        // !x  ->  BinaryExpression("!", 0, x)
        //
        // El backend/semantic analyzer podrá convertirlo
        // posteriormente en LogicalNot real.
        // ----------------------------------------------------

        auto zero =
            std::make_unique<IntegerLiteral>(0);

        return std::make_unique<BinaryExpression>(
            "!",
            std::move(zero),
            std::move(right));
    }


    // --------------------------------------------------------
    // UNARY MINUS
    // --------------------------------------------------------

    if (match(TokenType::Minus))
    {
        auto right =
            parseUnary();

        // ----------------------------------------------------
        // -x -> 0 - x
        // ----------------------------------------------------

        auto zero =
            std::make_unique<IntegerLiteral>(0);

        return std::make_unique<BinaryExpression>(
            "-",
            std::move(zero),
            std::move(right));
    }


    return parsePrimary();
}


// ============================================================
// PARSE PRIMARY
//
// 42
// x
// (42)
// (x + 10)
// ============================================================

std::unique_ptr<Expression>
Parser::parsePrimary()
{
    // ========================================================
    // INTEGER
    //
    // 42
    // ========================================================

    if (check(TokenType::Number))
    {
        int value =
            std::stoi(advance().text);

        return std::make_unique<IntegerLiteral>(
            value);
    }


    // ========================================================
    // VARIABLE
    //
    // x
    // ========================================================

    if (check(TokenType::Identifier))
    {
        std::string name =
            advance().text;

        return std::make_unique<VariableExpression>(
            name);
    }


    // ========================================================
    // PARENTHESES
    //
    // (x + 10)
    // ========================================================

    if (match(TokenType::LParen))
    {
        auto expression =
            parseExpression();

        expect(
            TokenType::RParen,
            "se esperaba ')'");

        return expression;
    }


    // ========================================================
    // ERROR
    // ========================================================

    throw std::runtime_error(
        "K++: expresión no reconocida");
}

}
