#pragma once

#include "AST.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
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
    // --------------------------------------------------------
    // SCOPES
    //
    // Cada bloque semántico posee su propia tabla de variables.
    //
    // scope[0] = scope de la función
    // scope[1] = bloque interno
    // scope[2] = bloque anidado
    //
    // nombre → tipo
    // --------------------------------------------------------

    using Scope =
        std::unordered_map<
            std::string,
            std::string
        >;

    std::vector<Scope> scopes;

    // --------------------------------------------------------
    // FUNCIONES
    //
    // nombre de función → función declarada
    //
    // Se registran todas las funciones antes de analizar
    // los cuerpos para permitir llamadas hacia adelante.
    // --------------------------------------------------------

    std::unordered_set<std::string> functionNames;

    // --------------------------------------------------------
    // Entrar en un nuevo scope
    // --------------------------------------------------------

    void enterScope();

    // --------------------------------------------------------
    // Salir del scope actual
    // --------------------------------------------------------

    void leaveScope();

    // --------------------------------------------------------
    // Buscar variable
    //
    // Busca desde el scope más interno hacia el externo.
    // --------------------------------------------------------

    const std::string* lookupVariable(
        const std::string& name) const;

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
