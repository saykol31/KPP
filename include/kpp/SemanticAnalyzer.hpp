#pragma once

#include "AST.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace kpp {

// ============================================================
// ERROR SEMÁNTICO
// ============================================================

struct SemanticError
{
    std::string message;
};

// ============================================================
// SEMANTIC ANALYZER
// ============================================================

class SemanticAnalyzer
{
public:

    // --------------------------------------------------------
    // Analizar programa completo
    // --------------------------------------------------------

    bool analyze(
        const Program& program);

    // --------------------------------------------------------
    // Obtener errores
    // --------------------------------------------------------

    const std::vector<SemanticError>&
    errors() const;

private:

    // --------------------------------------------------------
    // Función
    // --------------------------------------------------------

    void analyzeFunction(
        const FunctionDeclaration& function);

    // --------------------------------------------------------
    // Sentencia
    // --------------------------------------------------------

    void analyzeStatement(
        const Statement& statement);

    // --------------------------------------------------------
    // Expresión
    // --------------------------------------------------------

    void analyzeExpression(
        const Expression& expression);

    // --------------------------------------------------------
    // Registrar error
    // --------------------------------------------------------

    void error(
        const std::string& message);

    // --------------------------------------------------------
    // Tabla de variables
    //
    // nombre → tipo
    //
    // Ejemplo:
    //
    // x → int
    // y → int
    // --------------------------------------------------------

    std::unordered_map<
        std::string,
        std::string
    > variables;

    // --------------------------------------------------------
    // Errores encontrados
    // --------------------------------------------------------

    std::vector<SemanticError>
        semanticErrors;

    // --------------------------------------------------------
    // Función actualmente analizada
    // --------------------------------------------------------

    std::string currentFunction;
};

}
