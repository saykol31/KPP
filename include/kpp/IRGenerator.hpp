#pragma once

#include "AST.hpp"
#include "IR.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace kpp {

class IRGenerator
{
public:

    // ========================================================
    // GENERAR IR DEL PROGRAMA
    // ========================================================

    IRProgram generate(
        const Program& program);

private:

    // ========================================================
    // GENERAR IR DE UNA FUNCIÓN
    // ========================================================

    IRFunction generateFunction(
        const FunctionDeclaration& function);

    // ========================================================
    // GENERAR IR DE UNA SENTENCIA
    // ========================================================

    void generateStatement(
        const Statement& statement,
        IRFunction& function);

    // ========================================================
    // GENERAR IR DE UNA EXPRESIÓN
    // ========================================================

    std::string generateExpression(
        const Expression& expression,
        IRFunction& function);

    // ========================================================
    // CREAR TEMPORAL IR
    //
    // %0
    // %1
    // %2
    // ...
    // ========================================================

    std::string createTemporary();

    // ========================================================
    // CREAR ETIQUETA IR
    //
    // L0
    // L1
    // L2
    // ...
    //
    // Las etiquetas tienen su propio contador.
    // NO se mezclan con los temporales %0, %1, %2...
    // ========================================================

    std::string createLabel();

    // ========================================================
    // ÁMBITOS DE VARIABLES
    // Cada declaración local recibe un nombre IR único.
    // ========================================================

    std::vector<std::unordered_map<std::string, std::string>>
        variableScopes;

    int variableNameCounter = 0;

    // Tipo de estructura -> nombre de campo -> tipo del campo.
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::string>
    > structFields;

    void enterVariableScope()
    {
        variableScopes.emplace_back();
    }

    void leaveVariableScope()
    {
        if (!variableScopes.empty())
            variableScopes.pop_back();
    }

    std::string resolveVariableName(
        const std::string& name) const
    {
        for (auto it = variableScopes.rbegin();
             it != variableScopes.rend(); ++it)
        {
            auto found = it->find(name);
            if (found != it->end())
                return found->second;
        }

        // Parámetros y nombres administrados por el backend
        // conservan su nombre original si no están registrados.
        return name;
    }

    // ========================================================
    // CONTADOR DE TEMPORALES
    // ========================================================

    int temporaryCounter = 0;

    // ========================================================
    // CONTADOR DE ETIQUETAS
    // ========================================================

    int labelCounter = 0;
};

}
