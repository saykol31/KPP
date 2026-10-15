
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
        // ----------------------------------------------------
        // STRUCT DECLARATION
        // ----------------------------------------------------

        if (check(TokenType::KwStruct))
        {
            program->structs.push_back(
                parseStruct());

            continue;
        }

        // ----------------------------------------------------
        // FUNCTION DECLARATION
        // ----------------------------------------------------

        if (check(TokenType::KwEnum))
        {
            program->enums.push_back(
                parseEnum());

            continue;
        }

        if (check(TokenType::KwInt))
        {
            program->functions.push_back(
                parseFunction());

            continue;
        }

        throw std::runtime_error(
            "K++: se esperaba una declaración global "
            "(struct, enum o función int)");
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
    // PARÁMETROS
    //
    // int suma(int a, int b)
    //
    // FASE 4.2.1:
    // - declaración de parámetros
    // - solamente tipo int
    // - todavía no se procesan argumentos
    // --------------------------------------------------------

    std::vector<Parameter> parameters;

    if (!check(TokenType::RParen))
    {
        while (true)
        {
            // ------------------------------------------------
            // tipo
            // ------------------------------------------------

            expect(
                TokenType::KwInt,
                "se esperaba 'int' en parámetro");


            // ------------------------------------------------
            // nombre
            // ------------------------------------------------

            if (!check(TokenType::Identifier))
            {
                throw std::runtime_error(
                    "K++: se esperaba nombre de parámetro");
            }

            std::string parameterName =
                advance().text;


            // ------------------------------------------------
            // guardar parámetro
            // ------------------------------------------------

            parameters.push_back(
                Parameter{
                    "int",
                    parameterName
                });


            // ------------------------------------------------
            // siguiente parámetro
            // ------------------------------------------------

            if (!match(TokenType::Comma))
            {
                break;
            }
        }
    }


    // --------------------------------------------------------
    // )
    // --------------------------------------------------------

    expect(
        TokenType::RParen,
        "se esperaba ')' después de los parámetros");


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
    function->parameters = std::move(parameters);
    function->body = std::move(body);

    return function;
}


 // ============================================================
 // PARSE STRUCT
 // ============================================================

std::unique_ptr<StructDeclaration>
Parser::parseStruct()
{
    // --------------------------------------------------------
    // KEYWORD: struct
    // --------------------------------------------------------

    expect(
        TokenType::KwStruct,
        "se esperaba 'struct'");

    // --------------------------------------------------------
    // NOMBRE DE LA ESTRUCTURA
    // --------------------------------------------------------

    if (!check(TokenType::Identifier))
    {
        throw std::runtime_error(
            "K++: se esperaba nombre de estructura");
    }

    const std::string structName =
        advance().text;

    // --------------------------------------------------------
    // INICIO DEL CUERPO
    // --------------------------------------------------------

    expect(
        TokenType::LBrace,
        "se esperaba '{' después del nombre de estructura");

    auto declaration =
        std::make_unique<StructDeclaration>();

    declaration->name = structName;

    // --------------------------------------------------------
    // CAMPOS
    //
    // Tipos admitidos en esta etapa:
    //   int edad;
    //   string nombre;
    // --------------------------------------------------------

    while (
        !check(TokenType::RBrace)
        && !check(TokenType::EndOfFile))
    {
        std::string fieldType;

        if (match(TokenType::KwInt))
        {
            fieldType = "int";
        }
        else if (match(TokenType::KwString))
        {
            fieldType = "string";
        }
        else
        {
            throw std::runtime_error(
                "K++: tipo de campo no admitido en struct; "
                "se esperaba 'int' o 'string'");
        }

        if (!check(TokenType::Identifier))
        {
            throw std::runtime_error(
                "K++: se esperaba nombre del campo");
        }

        const std::string fieldName =
            advance().text;

        expect(
            TokenType::Semicolon,
            "se esperaba ';' después del campo");

        declaration->fields.push_back(
            StructField{
                fieldType,
                fieldName
            });
    }

    // --------------------------------------------------------
    // CIERRE DE LA ESTRUCTURA
    // --------------------------------------------------------

    expect(
        TokenType::RBrace,
        "se esperaba '}' al cerrar la estructura");

    // --------------------------------------------------------
    // TERMINADOR DE LA DECLARACIÓN
    // --------------------------------------------------------

    expect(
        TokenType::Semicolon,
        "se esperaba ';' después de la declaración struct");

    return declaration;
}


// ============================================================
// PARSE BLOCK
// ============================================================

std::unique_ptr<EnumDeclaration>
Parser::parseEnum()
{
    expect(TokenType::KwEnum, "se esperaba 'enum'");

    if (!check(TokenType::Identifier))
    {
        throw std::runtime_error(
            "K++: se esperaba nombre de enum");
    }

    auto declaration = std::make_unique<EnumDeclaration>();
    declaration->name = advance().text;

    expect(TokenType::LBrace, "se esperaba '{' después del nombre de enum");

    int nextValue = 0;

    while (!check(TokenType::RBrace)
           && !check(TokenType::EndOfFile))
    {
        if (!check(TokenType::Identifier))
        {
            throw std::runtime_error(
                "K++: se esperaba nombre de enumerador");
        }

        EnumValue value;
        value.name = advance().text;
        value.value = nextValue;

        if (match(TokenType::Assign))
        {
            if (!check(TokenType::Number))
            {
                throw std::runtime_error(
                    "K++: se esperaba número después de '=' en enum");
            }

            value.value = std::stoi(advance().text);
        }

        nextValue = value.value + 1;

        const std::string qualifiedName =
            declaration->name + "::" + value.name;

        if (enumConstants.count(qualifiedName) != 0)
        {
            throw std::runtime_error(
                "K++: enumerador duplicado: " + qualifiedName);
        }

        enumConstants[qualifiedName] = value.value;
        declaration->values.push_back(value);

        if (!check(TokenType::RBrace))
        {
            expect(TokenType::Comma,
                   "se esperaba ',' entre enumeradores");
        }
    }

    expect(TokenType::RBrace, "se esperaba '}' al cerrar enum");
    expect(TokenType::Semicolon,
           "se esperaba ';' después de la declaración enum");

    return declaration;
}


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
    // BLOQUE EXPLICITO
    //
    // {
    //     int x = 100;
    //     print_int(x);
    // }
    //
    // Permite scopes anidados explícitos.
    // ========================================================

    if (check(TokenType::LBrace))
    {
        return parseBlock();
    }

    // ========================================================
    // VARIABLE
    //
    // int x = 42;
    // int x = 10 + 20 * 3;
    // ========================================================
    std::string variableType;

    if (match(TokenType::KwInt))
    {
        variableType = "int";
    }
    else if (match(TokenType::KwString))
    {
        variableType = "string";
    }

    if (!variableType.empty())
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


        int arraySize = 0;

        if (match(TokenType::LBracket))
        {
            if (!check(TokenType::Number))
            {
                throw std::runtime_error(
                    "K++: se esperaba el tamaño del array");
            }

            arraySize = std::stoi(advance().text);

            if (arraySize <= 0)
            {
                throw std::runtime_error(
                    "K++: el tamaño del array debe ser positivo");
            }

            expect(
                TokenType::RBracket,
                "se esperaba ']' después del tamaño del array");
        }

        // ----------------------------------------------------
        // inicializador
        // ----------------------------------------------------

        std::unique_ptr<Expression> initializer = nullptr;

        if (match(TokenType::Assign))
        {
            initializer =
                parseExpression();
        }


        // ----------------------------------------------------
        // ;
        // ----------------------------------------------------

        expect(
            TokenType::Semicolon,
            "se esperaba ';'");


        return std::make_unique<VariableDeclaration>(
            variableType,
            name,
            std::move(initializer),
            arraySize);
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
    // DECLARACION DE TIPO PERSONALIZADO
    //
    // Persona p;
    // ========================================================

    if (check(TokenType::Identifier) &&
        position + 1 < tokens.size() &&
        tokens[position + 1].type == TokenType::Identifier)
    {
        std::string type = advance().text;
        std::string name = advance().text;

        expect(
            TokenType::Semicolon,
            "se esperaba ';' después de declaración de struct");

        return std::make_unique<VariableDeclaration>(
            type,
            name,
            nullptr);
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

        std::unique_ptr<Expression> index = nullptr;

        if (match(TokenType::LBracket))
        {
            index = parseExpression();

            expect(
                TokenType::RBracket,
                "se esperaba ']' después del índice del array");
        }

        // ----------------------------------------------------
        // Acceso a campo: p.edad
        // ----------------------------------------------------

        if (match(TokenType::Dot))
        {
            if (!check(TokenType::Identifier))
            {
                throw std::runtime_error(
                    "K++: se esperaba nombre de campo después de '.'");
            }

            name += "." + advance().text;
        }

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
            std::move(expression),
            std::move(index));
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

        std::unique_ptr<Expression> expression =
            parseExpression();

        expect(
            TokenType::RParen,
            "se esperaba ')'");

        expect(
            TokenType::Semicolon,
            "se esperaba ';'");

        return std::make_unique<PrintStatement>(
            std::move(expression));
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
    // STRING
    //
    // "Hola"
    // ========================================================

    if (check(TokenType::String))
    {
        std::string value =
            advance().text;

        return std::make_unique<StringLiteral>(
            value);
    }

    // ========================================================
    // VARIABLE
    //
    // x
    // ========================================================

    // ========================================================
    // VARIABLE / CALL EXPRESSION
    //
    // x
    // suma()
    // resta()
    // cualquierFuncion()
    //
    // FASE 4.1:
    // - llamadas sin parámetros
    // - devuelve int
    // ========================================================

    if (check(TokenType::Identifier))
    {
        std::string name =
            advance().text;

        // ----------------------------------------------------
        // ENUM CONSTANT: Color::Rojo
        // ----------------------------------------------------

        if (match(TokenType::ColonColon))
        {
            if (!check(TokenType::Identifier))
            {
                throw std::runtime_error(
                    "K++: se esperaba nombre de enumerador después de '::'");
            }

            name += "::" + advance().text;

            const auto it = enumConstants.find(name);

            if (it == enumConstants.end())
            {
                throw std::runtime_error(
                    "K++: enumerador no declarado: " + name);
            }

            return std::make_unique<IntegerLiteral>(it->second);
        }

        // ----------------------------------------------------
        // ARRAY ACCESS: valores[0]
        // ----------------------------------------------------

        if (match(TokenType::LBracket))
        {
            auto index = parseExpression();

            expect(
                TokenType::RBracket,
                "se esperaba ']' después del índice del array");

            return std::make_unique<ArrayAccessExpression>(
                name,
                std::move(index));
        }
        // ----------------------------------------------------
        // FUNCTION CALL
        // ----------------------------------------------------

        if (match(TokenType::LParen))
        {
            auto call =
                std::make_unique<CallExpression>(
                    name);

            // ------------------------------------------------
            // ARGUMENTS
            //
            // suma()
            // suma(10)
            // suma(10, 20)
            // suma(x, 20)
            // ------------------------------------------------

            if (!check(TokenType::RParen))
            {
                while (true)
                {
                    call->arguments.push_back(
                        parseExpression());

                    if (!match(TokenType::Comma))
                    {
                        break;
                    }
                }
            }

            expect(
                TokenType::RParen,
                "se esperaba ')' después de los argumentos");

            return call;
        }

        // ----------------------------------------------------
        // ACCESO A CAMPO: p.edad, p.nombre
        // ----------------------------------------------------

        if (match(TokenType::Dot))
        {
            if (!check(TokenType::Identifier))
            {
                throw std::runtime_error(
                    "K++: se esperaba nombre de campo después de '.'");
            }

            name += "." + advance().text;
        }

        // ----------------------------------------------------
        // VARIABLE
        // ----------------------------------------------------

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
