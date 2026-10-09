#pragma once

#include <memory>
#include <string>
#include <vector>

namespace kpp {

// ============================================================
// AST BASE
// ============================================================

struct ASTNode
{
    virtual ~ASTNode() = default;
};

// ============================================================
// EXPRESIONES
// ============================================================

struct Expression : ASTNode
{
};


struct IntegerLiteral : Expression
{
    int value;

    explicit IntegerLiteral(int value)
        : value(value)
    {
    }
};

struct StringLiteral : Expression
{
    std::string value;

    explicit StringLiteral(const std::string& value)
        : value(value)
    {
    }
};

// ============================================================
// VARIABLE EXPRESSION
// ============================================================

struct VariableExpression : Expression
{
    std::string name;

    explicit VariableExpression(
        const std::string& name)
        : name(name)
    {
    }
};

// ============================================================
// CALL EXPRESSION
//
// suma()
// resta()
// cualquierFuncion()
//
// FASE 4.1:
// - sin parámetros
// - devuelve int
// ============================================================

struct CallExpression : Expression
{
    std::string callee;

    std::vector<std::unique_ptr<Expression>> arguments;

    explicit CallExpression(
        const std::string& callee)
        : callee(callee)
    {
    }
};
//============================================================
// BINARY EXPRESSION
// ============================================================

struct BinaryExpression : Expression
{
    std::string op;

    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;

    BinaryExpression(
        const std::string& op,
        std::unique_ptr<Expression> left,
        std::unique_ptr<Expression> right)
        : op(op),
          left(std::move(left)),
          right(std::move(right))
    {
    }
};

// ============================================================
// SENTENCIAS
// ============================================================

struct Statement : ASTNode
{
};

struct ReturnStatement : Statement
{
    std::unique_ptr<Expression> expression;

    explicit ReturnStatement(
        std::unique_ptr<Expression> expression)
        : expression(std::move(expression))
    {
    }
};

struct PrintStatement : Statement
{
    std::unique_ptr<Expression> expression;

    explicit PrintStatement(
        std::unique_ptr<Expression> expression)
        : expression(std::move(expression))
    {
    }
};

// ============================================================
// PRINT INTEGER
// ============================================================

struct PrintIntegerStatement : Statement
{
    std::unique_ptr<Expression> expression;

    explicit PrintIntegerStatement(
        std::unique_ptr<Expression> expression)
        : expression(std::move(expression))
    {
    }
};

// ============================================================
// VARIABLE DECLARATION
// ============================================================

struct VariableDeclaration : Statement
{
    std::string type;
    std::string name;

    std::unique_ptr<Expression> initializer;

    VariableDeclaration(
        const std::string& type,
        const std::string& name,
        std::unique_ptr<Expression> initializer)
        : type(type),
          name(name),
          initializer(std::move(initializer))
    {
    }
};

// ============================================================
// ASSIGNMENT
// ============================================================

struct AssignmentStatement : Statement
{
    std::string name;

    std::unique_ptr<Expression> expression;

    AssignmentStatement(
        const std::string& name,
        std::unique_ptr<Expression> expression)
        : name(name),
          expression(std::move(expression))
    {
    }
};

// ============================================================
// BLOQUE
// ============================================================

struct Block : Statement
{
    std::vector<std::unique_ptr<Statement>> statements;
};

// ============================================================
// IF
// ============================================================

struct IfStatement : Statement
{
    std::unique_ptr<Expression> condition;

    std::unique_ptr<Block> thenBlock;

    std::unique_ptr<Block> elseBlock;

    IfStatement(
        std::unique_ptr<Expression> condition,
        std::unique_ptr<Block> thenBlock,
        std::unique_ptr<Block> elseBlock = nullptr)
        : condition(std::move(condition)),
          thenBlock(std::move(thenBlock)),
          elseBlock(std::move(elseBlock))
    {
    }
};

// ============================================================
// WHILE
// ============================================================

struct WhileStatement : Statement
{
    std::unique_ptr<Expression> condition;

    std::unique_ptr<Block> body;

    WhileStatement(
        std::unique_ptr<Expression> condition,
        std::unique_ptr<Block> body)
        : condition(std::move(condition)),
          body(std::move(body))
    {
    }
};

// ============================================================
// PARÁMETRO DE FUNCIÓN
// ============================================================

struct Parameter
{
    std::string type;
    std::string name;
};

// ============================================================
// FUNCIÓN
// ============================================================

struct FunctionDeclaration : ASTNode
{
    std::string returnType;
    std::string name;

    std::vector<Parameter> parameters;

    std::unique_ptr<Block> body;
};

// ============================================================
// PROGRAMA
// ============================================================

struct Program : ASTNode
{
    std::vector<std::unique_ptr<FunctionDeclaration>> functions;
};

}
