
#include "kpp/SemanticAnalyzer.hpp"

#include <iostream>

namespace kpp {

// ============================================================
// SCOPES
// ============================================================

void SemanticAnalyzer::enterScope()
{
    scopes.emplace_back();
}

void SemanticAnalyzer::leaveScope()
{
    if (!scopes.empty())
    {
        scopes.pop_back();
    }
}

const std::string*
SemanticAnalyzer::lookupVariable(
    const std::string& name) const
{
    for (auto it = scopes.rbegin();
         it != scopes.rend();
         ++it)
    {
        auto variable =
            it->find(name);

        if (variable != it->end())
        {
            return &variable->second;
        }
    }

    return nullptr;
}

// ============================================================
// ANALIZAR PROGRAMA
// ============================================================

bool SemanticAnalyzer::analyze(
    const Program& program)
{
    semanticErrors.clear();

    if (program.functions.empty())
    {
        semanticErrors.push_back({
            "el programa no contiene ninguna función"
        });

        return false;
    }

    bool hasMain = false;

    for (const auto& function :
         program.functions)
    {
        if (function->name == "main")
        {
            hasMain = true;
        }

        analyzeFunction(*function);
    }

    if (!hasMain)
    {
        semanticErrors.push_back({
            "el programa debe contener "
            "una función main()"
        });
    }

    return semanticErrors.empty();
}

// ============================================================
// ANALIZAR FUNCIÓN
// ============================================================

void SemanticAnalyzer::analyzeFunction(
    const FunctionDeclaration& function)
{
    currentFunction = function.name;

    // --------------------------------------------------------
    // Cada función comienza con su propio scope.
    // --------------------------------------------------------

    scopes.clear();
    enterScope();

    // --------------------------------------------------------
    // Verificar tipo de retorno
    // --------------------------------------------------------

    if (function.returnType != "int")
    {
        semanticErrors.push_back({
            "la función '" +
            function.name +
            "' tiene un tipo de retorno "
            "no soportado: " +
            function.returnType
        });
    }

    // --------------------------------------------------------
    // Verificar cuerpo
    // --------------------------------------------------------

    if (!function.body)
    {
        semanticErrors.push_back({
            "la función '" +
            function.name +
            "' no tiene cuerpo"
        });

        return;
    }

    for (const auto& statement :
         function.body->statements)
    {
        analyzeStatement(*statement);
    }

    // --------------------------------------------------------
    // Salir del scope de la función.
    // --------------------------------------------------------

    leaveScope();
}

// ============================================================
// ANALIZAR SENTENCIA
// ============================================================

void SemanticAnalyzer::analyzeStatement(
    const Statement& statement)
{
    // ========================================================
    // VARIABLE DECLARATION
    //
    // int x = 42;
    // ========================================================

    if (auto* variableDeclaration =
        dynamic_cast<const VariableDeclaration*>(
            &statement))
    {
        const std::string& name =
            variableDeclaration->name;

        // ----------------------------------------------------
        // Comprobar variable duplicada
        // ----------------------------------------------------

        if (scopes.back().find(name) !=
            scopes.back().end())
        {
            semanticErrors.push_back({
                "la variable '" +
                name +
                "' ya fue declarada en la función '" +
                currentFunction +
                "'"
            });

            return;
        }

        // ----------------------------------------------------
        // Debe existir un inicializador
        // ----------------------------------------------------

        if (!variableDeclaration->initializer)
        {
            semanticErrors.push_back({
                "la variable '" +
                name +
                "' no tiene inicializador"
            });

            return;
        }

        // ----------------------------------------------------
        // Analizar primero el inicializador
        //
        // Esto evita aceptar:
        //
        // int x = x;
        // ----------------------------------------------------

        analyzeExpression(
            *variableDeclaration->initializer);

        // ----------------------------------------------------
        // Registrar variable
        // ----------------------------------------------------

        if (scopes.back().find(name) ==
            scopes.back().end())
        {
            scopes.back()[name] = "int";
        }

        return;
    }

    // ========================================================
    // IF
    //
    // if (x > 5)
    // {
    //     print("IF OK");
    // }
    // ========================================================

    if (auto* ifStatement =
        dynamic_cast<const IfStatement*>(
            &statement))
    {
        // ----------------------------------------------------
        // La condición debe ser una expresión válida.
        // ----------------------------------------------------

        if (!ifStatement->condition)
        {
            semanticErrors.push_back({
                "if sin condición en función '" +
                currentFunction +
                "'"
            });

            return;
        }

        analyzeExpression(
            *ifStatement->condition);

        // ----------------------------------------------------
        // Analizar todas las sentencias del bloque.
        // ----------------------------------------------------

        if (!ifStatement->thenBlock)
        {
            semanticErrors.push_back({
                "if sin bloque en función '" +
                currentFunction +
                "'"
            });

            return;
        }

        // ----------------------------------------------------
        // El THEN posee su propio scope.
        // ----------------------------------------------------

        enterScope();

        for (const auto& nestedStatement :
             ifStatement->thenBlock->statements)
        {
            analyzeStatement(
                *nestedStatement);
        }

        leaveScope();

        // ----------------------------------------------------
        // Analizar ELSE, si existe.
        // Esto también permite analizar ELSE IF,
        // porque contiene otro IfStatement.
        // ----------------------------------------------------

        if (ifStatement->elseBlock)
        {
            // ------------------------------------------------
            // ELSE posee un scope independiente del THEN.
            // ------------------------------------------------

            enterScope();

            for (const auto& nestedStatement :
                 ifStatement->elseBlock->statements)
            {
                analyzeStatement(
                    *nestedStatement);
            }

            leaveScope();
        }

        return;
    }


    // ========================================================
    // RETURN
    // ========================================================

    if (auto* returnStatement =
        dynamic_cast<const ReturnStatement*>(
            &statement))
    {
        if (!returnStatement->expression)
        {
            semanticErrors.push_back({
                "return sin expresión en función '" +
                currentFunction +
                "'"
            });

            return;
        }

        analyzeExpression(
            *returnStatement->expression);

        return;
    }

    // ========================================================
    // PRINT
    // ========================================================

    if (auto* printStatement =
        dynamic_cast<const PrintStatement*>(
            &statement))
    {
        if (printStatement->text.empty())
        {
            semanticErrors.push_back({
                "print() no puede recibir una cadena vacía"
            });
        }

        return;
    }

    // ========================================================
    // SENTENCIA NO SOPORTADA
    // ========================================================

    semanticErrors.push_back({
        "sentencia no soportada en función '" +
        currentFunction +
        "'"
    });
}

// ============================================================
// ANALIZAR EXPRESIÓN
// ============================================================

void SemanticAnalyzer::analyzeExpression(
    const Expression& expression)
{
    // ========================================================
    // INTEGER
    // ========================================================

    if (dynamic_cast<const IntegerLiteral*>(
            &expression))
    {
        return;
    }

    // ========================================================
    // VARIABLE
    //
    // return x;
    // ========================================================

    if (auto* variable =
        dynamic_cast<const VariableExpression*>(
            &expression))
    {
        const std::string& name =
            variable->name;

        // ----------------------------------------------------
        // Buscar variable
        // ----------------------------------------------------

        const std::string* type =
            lookupVariable(name);

        if (type == nullptr)
        {
            semanticErrors.push_back({
                "Variable no declarada: " +
                name
            });

            return;
        }

        // ----------------------------------------------------
        // Actualmente todas nuestras variables son int
        // ----------------------------------------------------

        if (*type != "int")
        {
            semanticErrors.push_back({
                "tipo de variable no soportado: " +
                name
            });

            return;
        }

        return;
    }

    // ========================================================
    // BINARY EXPRESSION
    //
    // Ejemplos:
    //
    // x + y
    // x - y
    // x * y
    // x / y
    // x % y
    // x ^ y
    //
    // x == y
    // x != y
    // x < y
    // x <= y
    // x > y
    // x >= y
    //
    // x && y
    // x || y
    //
    // !x
    //
    // El parser representa !x como:
    //
    // BinaryExpression("!", 0, x)
    //
    // El - unario también llega como:
    //
    // BinaryExpression("-", 0, x)
    // ========================================================

    if (auto* binary =
        dynamic_cast<const BinaryExpression*>(
            &expression))
    {
        // ----------------------------------------------------
        // Analizar operando izquierdo
        // ----------------------------------------------------

        if (!binary->left)
        {
            semanticErrors.push_back({
                "expresión binaria sin operando izquierdo"
            });
        }
        else
        {
            analyzeExpression(
                *binary->left);
        }

        // ----------------------------------------------------
        // Analizar operando derecho
        // ----------------------------------------------------

        if (!binary->right)
        {
            semanticErrors.push_back({
                "expresión binaria sin operando derecho"
            });
        }
        else
        {
            analyzeExpression(
                *binary->right);
        }

        // ----------------------------------------------------
        // Operadores aritméticos
        // ----------------------------------------------------

        if (binary->op == "+" ||
            binary->op == "-" ||
            binary->op == "*" ||
            binary->op == "/" ||
            binary->op == "%")
        {
            return;
        }

        // ----------------------------------------------------
        // Potencia
        // ----------------------------------------------------

        if (binary->op == "^")
        {
            return;
        }

        // ----------------------------------------------------
        // Comparaciones
        // ----------------------------------------------------

        if (binary->op == "==" ||
            binary->op == "!=" ||
            binary->op == "<" ||
            binary->op == "<=" ||
            binary->op == ">" ||
            binary->op == ">=")
        {
            return;
        }

        // ----------------------------------------------------
        // Lógica
        // ----------------------------------------------------

        if (binary->op == "&&" ||
            binary->op == "||")
        {
            return;
        }

        // ----------------------------------------------------
        // NOT lógico
        //
        // El parser representa:
        //
        // !x
        //
        // como:
        //
        // BinaryExpression("!", 0, x)
        // ----------------------------------------------------

        if (binary->op == "!")
        {
            return;
        }

        // ----------------------------------------------------
        // Operador desconocido
        // ----------------------------------------------------

        semanticErrors.push_back({
            "operador no soportado: " +
            binary->op
        });

        return;
    }

    // ========================================================
    // EXPRESIÓN NO SOPORTADA
    // ========================================================

    semanticErrors.push_back({
        "expresión no soportada en función '" +
        currentFunction +
        "'"
    });
}

// ============================================================
// ERRORES
// ============================================================

const std::vector<SemanticError>&
SemanticAnalyzer::errors() const
{
    return semanticErrors;
}

} // namespace kpp
