
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
    functionNames.clear();
    functionDeclarations.clear();
    structFields.clear();

    // ========================================================
    // VALIDAR Y REGISTRAR DECLARACIONES STRUCT
    // ========================================================

    std::unordered_set<std::string> structNames;

    for (const auto& structure : program.structs)
    {
        if (!structure)
        {
            semanticErrors.push_back({
                "declaración struct inválida"
            });

            continue;
        }

        // ----------------------------------------------------
        // Comprobar nombre de estructura duplicado
        // ----------------------------------------------------

        if (!structNames.insert(structure->name).second)
        {
            semanticErrors.push_back({
                "la estructura '" +
                structure->name +
                "' está declarada más de una vez"
            });

            continue;
        }

        // ----------------------------------------------------
        // Comprobar nombres y tipos de los campos
        // ----------------------------------------------------

        std::unordered_set<std::string> fieldNames;

        for (const auto& field : structure->fields)
        {
            if (field.name.empty())
            {
                semanticErrors.push_back({
                    "campo sin nombre en la estructura '" +
                    structure->name + "'"
                });

                continue;
            }

            structFields[structure->name].emplace(
                field.name,
                field.type
            );

            if (!fieldNames.insert(field.name).second)
            {
                semanticErrors.push_back({
                    "el campo '" +
                    field.name +
                    "' está duplicado en la estructura '" +
                    structure->name + "'"
                });
            }

            if (field.type != "int" &&
                field.type != "string")
            {
                semanticErrors.push_back({
                    "tipo no soportado '" +
                    field.type +
                    "' en el campo '" +
                    field.name +
                    "' de la estructura '" +
                    structure->name + "'"
                });
            }
        }
    }

    if (program.functions.empty())
    {
        semanticErrors.push_back({
            "el programa no contiene ninguna función"
        });

        return false;
    }

    bool hasMain = false;

    // ========================================================
    // PRIMERA PASADA
    //
    // Registrar todas las funciones antes de analizar
    // sus cuerpos.
    //
    // Esto permite:
    //
    // int main()
    // {
    //     return suma();
    // }
    //
    // int suma()
    // {
    //     return 10;
    // }
    //
    // ========================================================

    for (const auto& function :
         program.functions)
    {
        if (function->name == "main")
        {
            hasMain = true;
        }

        if (!functionNames.insert(
                function->name).second)
        {
            semanticErrors.push_back({
                "la función '" +
                function->name +
                "' está declarada más de una vez"
            });
        }
        else
        {
            functionDeclarations[function->name] =
                function.get();
        }
    }

    // ========================================================
    // SEGUNDA PASADA
    //
    // Analizar los cuerpos de todas las funciones.
    // ========================================================

    for (const auto& function :
         program.functions)
    {
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
    // REGISTRAR PARÁMETROS DE LA FUNCIÓN
    //
    // Los parámetros forman parte del scope local de la
    // función y pueden utilizarse como variables:
    //
    // int suma(int a, int b)
    // {
    //     return a + b;
    // }
    //
    // a -> int
    // b -> int
    // --------------------------------------------------------

    for (const auto& parameter :
         function.parameters)
    {
        if (parameter.name.empty())
        {
            semanticErrors.push_back({
                "parámetro sin nombre en la función '" +
                function.name + "'"
            });

            continue;
        }

        if (scopes.back().find(parameter.name) !=
            scopes.back().end())
        {
            semanticErrors.push_back({
                "parámetro duplicado '" +
                parameter.name +
                "' en la función '" +
                function.name + "'"
            });

            continue;
        }

        scopes.back()[parameter.name] =
            parameter.type;
    }

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
        // Inicializador opcional
        //
        // int x;
        // int x = 10;
        //
        // Analizar primero el inicializador cuando existe.
        // Esto evita aceptar:
        //
        // int x = x;
        // ----------------------------------------------------

        if (variableDeclaration->initializer)
        {
            analyzeExpression(
                *variableDeclaration->initializer);
        }

        // ----------------------------------------------------
        // Registrar variable
        // ----------------------------------------------------

        const std::string& declaredType =
            variableDeclaration->type;

        if (declaredType != "int" &&
            declaredType != "string" &&
            structFields.find(declaredType) == structFields.end())
        {
            semanticErrors.push_back({
                "tipo no declarado o no soportado '" +
                declaredType + "' para la variable '" +
                name + "'"
            });
            return;
        }

        scopes.back()[name] = declaredType;

        return;
    }

    // ========================================================
    // ASSIGNMENT
    //
    // x = x - 1;
    // ========================================================

    if (auto* assignmentStatement =
        dynamic_cast<const AssignmentStatement*>(
            &statement))
    {
        const std::string& name =
            assignmentStatement->name;

        // ----------------------------------------------------
        // La variable debe existir.
        // ----------------------------------------------------

        const std::size_t dot = name.find('.');
        const std::string baseName =
            dot == std::string::npos
                ? name
                : name.substr(0, dot);

        const std::string* baseType = lookupVariable(baseName);

        if (baseType == nullptr)
        {
            semanticErrors.push_back({
                "la variable '" + baseName +
                "' no fue declarada en la función '" +
                currentFunction + "'"
            });
            return;
        }

        if (dot != std::string::npos)
        {
            const std::string fieldName = name.substr(dot + 1);
            const auto structure = structFields.find(*baseType);

            if (structure == structFields.end())
            {
                semanticErrors.push_back({
                    "la variable '" + baseName +
                    "' no es una estructura"
                });
                return;
            }

            if (structure->second.find(fieldName) ==
                structure->second.end())
            {
                semanticErrors.push_back({
                    "el campo '" + fieldName +
                    "' no existe en la estructura '" +
                    *baseType + "'"
                });
                return;
            }
        }

        // ----------------------------------------------------
        // La asignación debe tener una expresión.
        // ----------------------------------------------------

        if (!assignmentStatement->expression)
        {
            semanticErrors.push_back({
                "la asignación de '" +
                name +
                "' no tiene expresión"
            });

            return;
        }

        // ----------------------------------------------------
        // Analizar la expresión.
        // ----------------------------------------------------

        analyzeExpression(
            *assignmentStatement->expression);

        return;
    }


    // ========================================================
    // BLOCK
    //
    // {
    //     int x = 100;
    //     print_int(x);
    // }
    //
    // Un bloque explícito crea un scope independiente.
    // Esto permite shadowing de variables:
    //
    // int x = 20;
    // {
    //     int x = 100;
    // }
    // ========================================================

    if (auto* block =
            dynamic_cast<const Block*>(&statement))
    {
        // ----------------------------------------------------
        // El bloque posee su propio scope.
        // ----------------------------------------------------

        enterScope();

        // ----------------------------------------------------
        // Analizar cada sentencia del bloque.
        // ----------------------------------------------------

        for (const auto& nestedStatement :
             block->statements)
        {
            analyzeStatement(
                *nestedStatement);
        }

        // ----------------------------------------------------
        // Destruir el scope al salir del bloque.
        // ----------------------------------------------------

        leaveScope();

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
    // WHILE
    //
    // while (condición)
    // {
    //     ...
    // }
    // ========================================================

    if (auto* whileStatement =
        dynamic_cast<const WhileStatement*>(
            &statement))
    {
        // ----------------------------------------------------
        // La condición debe existir.
        // ----------------------------------------------------

        if (!whileStatement->condition)
        {
            semanticErrors.push_back({
                "while sin condición en función '" +
                currentFunction +
                "'"
            });

            return;
        }

        // ----------------------------------------------------
        // Analizar condición.
        // ----------------------------------------------------

        analyzeExpression(
            *whileStatement->condition);

        // ----------------------------------------------------
        // El cuerpo debe existir.
        // ----------------------------------------------------

        if (!whileStatement->body)
        {
            semanticErrors.push_back({
                "while sin bloque en función '" +
                currentFunction +
                "'"
            });

            return;
        }

        // ----------------------------------------------------
        // El cuerpo posee su propio scope.
        // ----------------------------------------------------

        enterScope();

        for (const auto& nestedStatement :
             whileStatement->body->statements)
        {
            analyzeStatement(
                *nestedStatement);
        }

        leaveScope();

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
        if (!printStatement->expression)
        {
            semanticErrors.push_back({
                "print() requiere una expresión"
            });

            return;
        }

        analyzeExpression(
            *printStatement->expression);

        return;
    }

    // ========================================================
    // PRINT INTEGER
    // ========================================================
    //
    // print_int(x);
    // print_int(10);
    // print_int(x + 1);
    //
    // La expresión se valida semánticamente igual que
    // cualquier otra expresión del lenguaje.
    // ========================================================

    if (auto* printIntegerStatement =
        dynamic_cast<const PrintIntegerStatement*>(
            &statement))
    {
        if (!printIntegerStatement->expression)
        {
            semanticErrors.push_back({
                "print_int() requiere una expresión"
            });

            return;
        }

        analyzeExpression(
            *printIntegerStatement->expression);

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
    // STRING
    // ========================================================

    if (dynamic_cast<const StringLiteral*>(&expression))
    {
        return;
}
    // ========================================================
    // CALL EXPRESSION
    //
    // return suma();
    // return cualquierFuncion();
    //
    // FASE 4.1:
    // - sin parámetros
    // - devuelve int
    // ========================================================

    // ========================================================
    // CALL EXPRESSION
    //
    // return suma();
    // return suma(10, 20);
    // return suma(x, 20);
    //
    // FASE 4.2.3:
    // - verificar existencia de la función
    // - verificar cantidad de argumentos
    // - analizar cada argumento
    // - relacionar argumento[i] con parámetro[i]
    // ========================================================

    if (auto* call =
        dynamic_cast<const CallExpression*>(
            &expression))
    {
        // ----------------------------------------------------
        // Verificar que la función exista.
        // ----------------------------------------------------

        if (functionNames.find(call->callee) ==
            functionNames.end())
        {
            semanticErrors.push_back({
                "función no declarada: " +
                call->callee
            });

            return;
        }

        // ----------------------------------------------------
        // Obtener la declaración de la función.
        // ----------------------------------------------------

        auto functionIt =
            functionDeclarations.find(
                call->callee);

        if (functionIt ==
            functionDeclarations.end())
        {
            semanticErrors.push_back({
                "no se pudo obtener la declaración de la función: " +
                call->callee
            });

            return;
        }

        const FunctionDeclaration& function =
            *functionIt->second;

        // ----------------------------------------------------
        // Verificar cantidad de argumentos.
        //
        // suma(int a, int b)
        //
        // suma(10, 20)
        //      2       2
        // ----------------------------------------------------

        if (call->arguments.size() !=
            function.parameters.size())
        {
            semanticErrors.push_back({
                "la función '" +
                call->callee +
                "' requiere " +
                std::to_string(
                    function.parameters.size()) +
                " argumento(s), pero recibió " +
                std::to_string(
                    call->arguments.size())
            });

            return;
        }

        // ----------------------------------------------------
        // Relacionar cada argumento con su parámetro.
        //
        // argumento[0] ↔ parámetro[0]
        // argumento[1] ↔ parámetro[1]
        // ...
        //
        // Actualmente todos los parámetros y argumentos
        // válidos del lenguaje son int.
        // ----------------------------------------------------

        for (std::size_t i = 0;
             i < call->arguments.size();
             ++i)
        {
            const auto& argument =
                call->arguments[i];

            if (!argument)
            {
                semanticErrors.push_back({
                    "argumento nulo en la posición " +
                    std::to_string(i + 1) +
                    " de la función '" +
                    call->callee +
                    "'"
                });

                continue;
            }

            // ----------------------------------------------
            // Analizar expresión del argumento.
            //
            // Esto valida:
            //
            // suma(x, 10)
            // suma(10 + 20, x)
            // suma(suma(), 20)
            // ----------------------------------------------

            analyzeExpression(*argument);

            // ----------------------------------------------
            // Actualmente el tipo del parámetro es int.
            // La comprobación de tipos de argumentos se
            // ampliará cuando el lenguaje tenga más tipos.
            // ----------------------------------------------

            const Parameter& parameter =
                function.parameters[i];

            if (parameter.type != "int")
            {
                semanticErrors.push_back({
                    "tipo de parámetro no soportado: " +
                    parameter.type +
                    " en la función '" +
                    call->callee +
                    "'"
                });
            }
        }

        // ----------------------------------------------------
        // Las funciones actuales devuelven int.
        // ----------------------------------------------------

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

        const std::size_t dot = name.find('.');
        const std::string baseName =
            dot == std::string::npos
                ? name
                : name.substr(0, dot);

        const std::string* type = lookupVariable(baseName);

        if (type == nullptr)
        {
            semanticErrors.push_back({
                "Variable no declarada: " + baseName
            });
            return;
        }

        std::string valueType = *type;

        if (dot != std::string::npos)
        {
            const std::string fieldName = name.substr(dot + 1);
            const auto structure = structFields.find(*type);

            if (structure == structFields.end())
            {
                semanticErrors.push_back({
                    "la variable '" + baseName +
                    "' no es una estructura"
                });
                return;
            }

            const auto field = structure->second.find(fieldName);

            if (field == structure->second.end())
            {
                semanticErrors.push_back({
                    "el campo '" + fieldName +
                    "' no existe en la estructura '" +
                    *type + "'"
                });
                return;
            }

            valueType = field->second;
        }

        if (valueType != "int" && valueType != "string")
        {
            semanticErrors.push_back({
                "tipo de variable no soportado: " + name
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

