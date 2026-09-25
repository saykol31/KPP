#pragma once

#include "AST.hpp"
#include "IR.hpp"

#include <string>

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
    // CONTADOR DE TEMPORALES
    // ========================================================

    int temporaryCounter = 0;

    // ========================================================
    // CONTADOR DE ETIQUETAS
    // ========================================================

    int labelCounter = 0;
};

}
