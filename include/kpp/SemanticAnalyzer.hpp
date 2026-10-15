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

    // Inferir el tipo de una expresión. Cadena vacía si no se puede
    // determinar (por ejemplo, porque ya existe un error semántico).
    std::string inferExpressionType(
        const Expression& expression) const;

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

    // Tamaño de cada array por ámbito.
    // 0 = variable normal; positivo = array.
    using ArrayScope =
        std::unordered_map<std::string, int>;

    std::vector<ArrayScope> arrayScopes;

    int lookupArraySize(
        const std::string& name) const;
    // nombre de struct -> (nombre de campo -> tipo)
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::string>
    > structFields;

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
    // DECLARACIONES DE FUNCIONES
    //
    // nombre de función → declaración AST
    //
    // Permite consultar los parámetros de una función
    // cuando analizamos una llamada.
    // --------------------------------------------------------

    std::unordered_map<
        std::string,
        const FunctionDeclaration*> functionDeclarations;

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
