
#include "kpp/IRGenerator.hpp"

#include <stdexcept>

namespace kpp {

// ============================================================
// NOMBRE DE OPCODE
// ============================================================

const char* irOpcodeName(IROpcode opcode)
{
    switch (opcode)
    {
        // ====================================================
        // ESTRUCTURA DEL PROGRAMA
        // ====================================================

        case IROpcode::FunctionBegin:
            return "FUNCTION_BEGIN";

        case IROpcode::FunctionEnd:
            return "FUNCTION_END";


        // ====================================================
        // CONSTANTES
        // ====================================================

        case IROpcode::Const:
            return "CONST";

        // ====================================================
        // VARIABLES Y MEMORIA
        // ====================================================

        case IROpcode::Declare:
            return "DECLARE";

        case IROpcode::Store:
            return "STORE";

        case IROpcode::Load:
            return "LOAD";

        // ====================================================
        // ARITMÉTICA BÁSICA
        // ====================================================

        case IROpcode::Add:
            return "ADD";

        case IROpcode::Sub:
            return "SUB";

        case IROpcode::Mul:
            return "MUL";

        case IROpcode::Div:
            return "DIV";

        case IROpcode::Mod:
            return "MOD";

        // ====================================================
        // POTENCIAS Y RAÍCES
        // ====================================================

        case IROpcode::Pow:
            return "POW";

        case IROpcode::Sqrt:
            return "SQRT";

        // ====================================================
        // LOGARITMOS Y EXPONENCIALES
        // ====================================================

        case IROpcode::Log:
            return "LOG";

        case IROpcode::Log10:
            return "LOG10";

        case IROpcode::Exp:
            return "EXP";

        // ====================================================
        // TRIGONOMETRÍA
        // ====================================================

        case IROpcode::Sin:
            return "SIN";

        case IROpcode::Cos:
            return "COS";

        case IROpcode::Tan:
            return "TAN";

        case IROpcode::Cot:
            return "COT";

        case IROpcode::Sec:
            return "SEC";

        case IROpcode::Csc:
            return "CSC";

        // ====================================================
        // TRIGONOMETRÍA INVERSA
        // ====================================================

        case IROpcode::Asin:
            return "ASIN";

        case IROpcode::Acos:
            return "ACOS";

        case IROpcode::Atan:
            return "ATAN";

        // ====================================================
        // FUNCIONES MATEMÁTICAS
        // ====================================================

        case IROpcode::Abs:
            return "ABS";

        case IROpcode::Floor:
            return "FLOOR";

        case IROpcode::Ceil:
            return "CEIL";

        case IROpcode::Round:
            return "ROUND";

        case IROpcode::Min:
            return "MIN";

        case IROpcode::Max:
            return "MAX";

        // ====================================================
        // MATEMÁTICA AVANZADA
        // ====================================================

        case IROpcode::Factorial:
            return "FACTORIAL";

        case IROpcode::Sum:
            return "SUM";

        case IROpcode::Integral:
            return "INTEGRAL";

        // ====================================================
        // FUNCIONES
        // ====================================================

        case IROpcode::Call:
            return "CALL";

        case IROpcode::Return:
            return "RETURN";

        // ====================================================
        // SALIDA
        // ====================================================

        case IROpcode::Print:
            return "PRINT";

        case IROpcode::PrintInteger:
            return "PRINT_INT";

        case IROpcode::PrintFloat:
            return "PRINT_FLOAT";

        // ====================================================
        // COMPARACIONES
        // ====================================================

        case IROpcode::CompareEqual:
            return "CMP_EQ";

        case IROpcode::CompareNotEqual:
            return "CMP_NE";

        case IROpcode::CompareLess:
            return "CMP_LT";

        case IROpcode::CompareLessEqual:
            return "CMP_LE";

        case IROpcode::CompareGreater:
            return "CMP_GT";

        case IROpcode::CompareGreaterEqual:
            return "CMP_GE";

        // ====================================================
        // LÓGICA
        // ====================================================

        case IROpcode::LogicalAnd:
            return "AND";

        case IROpcode::LogicalOr:
            return "OR";

        case IROpcode::LogicalNot:
            return "NOT";

        // ====================================================
        // CONTROL DE FLUJO
        // ====================================================

        case IROpcode::Label:
            return "LABEL";

        case IROpcode::Jump:
            return "JUMP";

        case IROpcode::JumpIfZero:
            return "JUMP_IF_ZERO";

        case IROpcode::JumpIfNotZero:
            return "JUMP_IF_NOT_ZERO";
    }

    return "UNKNOWN";
}

// ============================================================
// CREAR TEMPORAL
//
// %0
// %1
// %2
// ...
// ============================================================

std::string IRGenerator::createTemporary()
{
    return "%" + std::to_string(temporaryCounter++);
}

// ============================================================
// CREAR ETIQUETA IR
//
// L0
// L1
// L2
// ...
//
// Las etiquetas son independientes de los temporales.
// ============================================================

std::string IRGenerator::createLabel()
{
    return "L" + std::to_string(labelCounter++);
}

// ============================================================
// GENERAR PROGRAMA IR
// ============================================================

IRProgram IRGenerator::generate(
    const Program& program)
{
    IRProgram ir;

    for (const auto& function :
         program.functions)
    {
        ir.functions.push_back(
            generateFunction(*function));
    }

    return ir;
}

// ============================================================
// GENERAR FUNCIÓN
// ============================================================

IRFunction IRGenerator::generateFunction(
    const FunctionDeclaration& function)
{
    IRFunction irFunction;

    irFunction.name = function.name;

    // --------------------------------------------------------
    // Cada función comienza sus temporales desde %0
    // --------------------------------------------------------

    temporaryCounter = 0;

    // --------------------------------------------------------
    // Cada función comienza sus etiquetas desde L0
    // --------------------------------------------------------

    labelCounter = 0;

    // --------------------------------------------------------
    // FUNCTION BEGIN
    // --------------------------------------------------------

    irFunction.instructions.push_back({
        IROpcode::FunctionBegin,
        function.name,
        "",
        "",
        0
    });

    // --------------------------------------------------------
    // BODY
    // --------------------------------------------------------

    if (!function.body)
    {
        throw std::runtime_error(
            "K++ IR: función sin cuerpo");
    }

    for (const auto& statement :
         function.body->statements)
    {
        generateStatement(
            *statement,
            irFunction);
    }

    // --------------------------------------------------------
    // FUNCTION END
    // --------------------------------------------------------

    irFunction.instructions.push_back({
        IROpcode::FunctionEnd,
        "",
        "",
        "",
        0
    });

    return irFunction;
}

// ============================================================
// GENERAR SENTENCIA
// ============================================================

void IRGenerator::generateStatement(
    const Statement& statement,
    IRFunction& function)
{
    // ========================================================
    // VARIABLE DECLARATION
    //
    // int x = 42;
    //
    // ->
    //
    // DECLARE x
    // STORE x, 42
    // ========================================================

    if (auto* variableDeclaration =
        dynamic_cast<const VariableDeclaration*>(
            &statement))
    {
        // ----------------------------------------------------
        // DECLARE
        // ----------------------------------------------------

        function.instructions.push_back({
            IROpcode::Declare,
            variableDeclaration->name,
            "",
            "",
            0
        });

        // ----------------------------------------------------
        // Verificar inicializador
        // ----------------------------------------------------

        if (!variableDeclaration->initializer)
        {
            throw std::runtime_error(
                "K++ IR: variable sin inicializador");
        }

        // ----------------------------------------------------
        // INITIALIZER
        //
        // Generamos cualquier expresión soportada.
        //
        // int x = 42;
        // int x = y;
        // int x = 10 + 20;
        // int x = 10 + 20 * 3;
        // ----------------------------------------------------

        const Expression& initializer =
            *variableDeclaration->initializer;

        // ----------------------------------------------------
        // Literal entero
        //
        // int x = 42;
        //
        // ->
        //
        // CONST 42 -> %0
        // STORE x <- %0
        //
        // STORE siempre recibe un temporal.
        // ----------------------------------------------------

        if (auto* integer =
            dynamic_cast<const IntegerLiteral*>(
                &initializer))
        {
            std::string temporary =
                createTemporary();

            function.instructions.push_back({
                IROpcode::Const,
                "",
                "",
                temporary,
                integer->value
            });

            function.instructions.push_back({
                IROpcode::Store,
                variableDeclaration->name,
                temporary,
                "",
                0
            });

            return;
        }

        // ----------------------------------------------------
        // Cualquier otra expresión
        // ----------------------------------------------------

        std::string temporary =
            generateExpression(
                initializer,
                function);

        function.instructions.push_back({
            IROpcode::Store,
            variableDeclaration->name,
            temporary,
            "",
            0
        });

        return;
    }

    // ========================================================
    // ASSIGNMENT
    //
    // x = x - 1;
    //
    // ->
    //
    // LOAD x -> %N
    // CONST 1 -> %N
    // SUB %N, %N -> %N
    // STORE x <- %N
    // ========================================================

    if (auto* assignmentStatement =
            dynamic_cast<const AssignmentStatement*>(&statement))
    {
        // ----------------------------------------------------
        // Verificar expresión
        // ----------------------------------------------------

        if (!assignmentStatement->expression)
        {
            throw std::runtime_error(
                "K++ IR: asignación sin expresión");
        }

        // ----------------------------------------------------
        // Generar la expresión completa.
        //
        // Esto permite:
        //
        // x = 10;
        // x = y;
        // x = x - 1;
        // x = a + b * 3;
        // ----------------------------------------------------

        std::string temporary =
            generateExpression(
                *assignmentStatement->expression,
                function);

        // ----------------------------------------------------
        // STORE
        // ----------------------------------------------------

        function.instructions.push_back({
            IROpcode::Store,
            assignmentStatement->name,
            temporary,
            "",
            0
        });

        return;
    }

    // ========================================================
    // IF
    //
    // if (x > 5)
    // {
    //     print("IF OK");
    // }
    //
    // ->
    //
    // LOAD x -> %0
    // CONST 5 -> %1
    // CMP_GT %0, %1 -> %2
    // JUMP_IF_ZERO %2 -> L0
    //
    // PRINT "IF OK"
    //
    // LABEL L0
    // ========================================================

    if (auto* ifStatement =
        dynamic_cast<const IfStatement*>(&statement))
    {
        // ----------------------------------------------------
        // Verificar condición
        // ----------------------------------------------------

        if (!ifStatement->condition)
        {
            throw std::runtime_error(
                "K++ IR: if sin condición");
        }

        // ----------------------------------------------------
        // Generar condición
        //
        // El resultado debe ser un temporal.
        // ----------------------------------------------------

        std::string conditionTemporary =
            generateExpression(
                *ifStatement->condition,
                function);

        // ----------------------------------------------------
        // Crear etiquetas de control
        //
        // Sin ELSE:
        //
        //     JUMP_IF_ZERO %cond -> endLabel
        //     THEN
        //     LABEL endLabel
        //
        // Con ELSE:
        //
        //     JUMP_IF_ZERO %cond -> elseLabel
        //     THEN
        //     JUMP -> endLabel
        //     LABEL elseLabel
        //     ELSE
        //     LABEL endLabel
        //
        // ----------------------------------------------------

        std::string endLabel =
            createLabel();

        std::string elseLabel =
            createLabel();

        const bool hasElse =
            (ifStatement->elseBlock != nullptr);

        function.instructions.push_back({
            IROpcode::JumpIfZero,
            hasElse ? elseLabel : endLabel,
            conditionTemporary,
            "",
            0
        });

        // ----------------------------------------------------
        // Generar bloque THEN
        // ----------------------------------------------------

        if (!ifStatement->thenBlock)
        {
            throw std::runtime_error(
                "K++ IR: if sin bloque");
        }

        for (const auto& nestedStatement :
             ifStatement->thenBlock->statements)
        {
            generateStatement(
                *nestedStatement,
                function);
        }

        // ----------------------------------------------------
        // Si existe ELSE, saltar el bloque ELSE
        // después de ejecutar THEN.
        // ----------------------------------------------------

        if (hasElse)
        {
            function.instructions.push_back({
                IROpcode::Jump,
                endLabel,
                "",
                "",
                0
            });

            // ------------------------------------------------
            // LABEL ELSE
            // ------------------------------------------------

            function.instructions.push_back({
                IROpcode::Label,
                elseLabel,
                "",
                "",
                0
            });

            // ------------------------------------------------
            // Generar bloque ELSE
            // ------------------------------------------------

            for (const auto& nestedStatement :
                 ifStatement->elseBlock->statements)
            {
                generateStatement(
                    *nestedStatement,
                    function);
            }
        }

        // ----------------------------------------------------
        // LABEL de salida
        // ----------------------------------------------------

        function.instructions.push_back({
            IROpcode::Label,
            endLabel,
            "",
            "",
            0
        });

        return;
    }

    // ========================================================
    // WHILE
    //
    // LABEL startLabel
    // condición
    // JUMP_IF_ZERO condición -> endLabel
    // cuerpo
    // JUMP -> startLabel
    // LABEL endLabel
    // ========================================================

    if (auto* whileStatement =
        dynamic_cast<const WhileStatement*>(&statement))
    {
        // ----------------------------------------------------
        // Verificar condición
        // ----------------------------------------------------

        if (!whileStatement->condition)
        {
            throw std::runtime_error(
                "K++ IR: while sin condición");
        }

        // ----------------------------------------------------
        // Verificar bloque
        // ----------------------------------------------------

        if (!whileStatement->body)
        {
            throw std::runtime_error(
                "K++ IR: while sin bloque");
        }

        // ----------------------------------------------------
        // Crear etiquetas
        // ----------------------------------------------------

        std::string startLabel =
            createLabel();

        std::string endLabel =
            createLabel();

        // ----------------------------------------------------
        // LABEL inicio
        // ----------------------------------------------------

        function.instructions.push_back({
            IROpcode::Label,
            startLabel,
            "",
            "",
            0
        });

        // ----------------------------------------------------
        // Generar condición
        // ----------------------------------------------------

        std::string conditionTemporary =
            generateExpression(
                *whileStatement->condition,
                function);

        // ----------------------------------------------------
        // Si condición == 0, salir del ciclo
        // ----------------------------------------------------

        function.instructions.push_back({
            IROpcode::JumpIfZero,
            endLabel,
            conditionTemporary,
            "",
            0
        });

        // ----------------------------------------------------
        // Generar cuerpo
        // ----------------------------------------------------

        for (const auto& nestedStatement :
             whileStatement->body->statements)
        {
            generateStatement(
                *nestedStatement,
                function);
        }

        // ----------------------------------------------------
        // Volver al inicio
        // ----------------------------------------------------

        function.instructions.push_back({
            IROpcode::Jump,
            startLabel,
            "",
            "",
            0
        });

        // ----------------------------------------------------
        // LABEL salida
        // ----------------------------------------------------

        function.instructions.push_back({
            IROpcode::Label,
            endLabel,
            "",
            "",
            0
        });

        return;
    }

    // ========================================================
    // PRINT
    // ========================================================

    if (auto* printStatement =
        dynamic_cast<const PrintStatement*>(&statement))
    {
        function.instructions.push_back({
            IROpcode::Print,
            printStatement->text,
            "",
            "",
            0
        });

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
    // Primero generamos la expresión para obtener
    // el temporal que contiene el valor entero.
    // ========================================================

    if (auto* printIntegerStatement =
        dynamic_cast<const PrintIntegerStatement*>(
            &statement))
    {
        if (!printIntegerStatement->expression)
        {
            throw std::runtime_error(
                "K++ IR: print_int sin expresión");
        }

        std::string temporary =
            generateExpression(
                *printIntegerStatement->expression,
                function);

        function.instructions.push_back({
            IROpcode::PrintInteger,
            "",
            temporary,
            "",
            0
        });

        return;
    }

    // ========================================================
    // RETURN
    // ========================================================

    if (auto* returnStatement =
        dynamic_cast<const ReturnStatement*>(
            &statement))
    {
        // ----------------------------------------------------
        // Verificar que exista expresión
        // ----------------------------------------------------

        if (!returnStatement->expression)
        {
            throw std::runtime_error(
                "K++ IR: return sin expresión");
        }

        // ----------------------------------------------------
        // Literal entero
        //
        // return 42;
        // ----------------------------------------------------

        if (auto* integer =
            dynamic_cast<const IntegerLiteral*>(
                returnStatement->expression.get()))
        {
            function.instructions.push_back({
                IROpcode::Return,
                "",
                "",
                "",
                integer->value
            });

            return;
        }

        // ----------------------------------------------------
        // Cualquier otra expresión
        //
        // return x;
        // return x + y;
        // return 10 + 20 * 3;
        // ----------------------------------------------------

        std::string temporary =
            generateExpression(
                *returnStatement->expression,
                function);

        // ----------------------------------------------------
        // PRUEBA DE FUEGO
        //
        // Mostrar expresiones enteras antes de retornar.
        // ----------------------------------------------------

        function.instructions.push_back({
            IROpcode::PrintInteger,
            "",
            temporary,
            "",
            0
        });

        function.instructions.push_back({
            IROpcode::Return,
            "",
            temporary,
            "",
            0
        });

        return;
    }

    // ========================================================
    // SENTENCIA NO SOPORTADA
    // ========================================================

    throw std::runtime_error(
        "K++ IR: sentencia no soportada");
}

// ============================================================
// GENERAR EXPRESIÓN
// ============================================================

std::string IRGenerator::generateExpression(
    const Expression& expression,
    IRFunction& function)
{

    // ========================================================
    // LITERAL ENTERO
    //
    // 10
    //
    // ->
    //
    // CONST 10 -> %0
    //
    // Los literales usados dentro de expresiones no son
    // variables. Por eso utilizan CONST y no STORE.
    // ========================================================



    if (auto* integer =
        dynamic_cast<const IntegerLiteral*>(
            &expression))
    {
        std::string result =
            createTemporary();

        function.instructions.push_back({
            IROpcode::Const,
            "",
            "",
            result,
            integer->value
        });

        return result;
    }

    // ========================================================
    // CALL EXPRESSION
    //
    // suma()
    // resta()
    // cualquierFuncion()
    //
    // ->
    //
    // CALL suma -> %0
    //
    // FASE 4.1:
    // - sin parámetros
    // - devuelve int
    // ========================================================

    if (auto* call =
        dynamic_cast<const CallExpression*>(
            &expression))
    {
        std::string result =
            createTemporary();

        function.instructions.push_back({
            IROpcode::Call,
            call->callee,
            "",
            result,
            0
        });

        return result;
    }

    // ========================================================
    // VARIABLE
    //
    // x
    //
    // ->
    //
    // LOAD x -> %0
    // ========================================================

    if (auto* variable =
        dynamic_cast<const VariableExpression*>(
            &expression))
    {
        std::string temporary =
            createTemporary();

        function.instructions.push_back({
            IROpcode::Load,
            variable->name,
            "",
            temporary,
            0
        });

        return temporary;
    }

    // ========================================================
    // BINARY EXPRESSION
    // ========================================================

    if (auto* binary =
        dynamic_cast<const BinaryExpression*>(
            &expression))
    {
        // ----------------------------------------------------
        // Validar operandos
        // ----------------------------------------------------

        if (!binary->left)
        {
            throw std::runtime_error(
                "K++ IR: expresión sin operando izquierdo");
        }

        if (!binary->right)
        {
            throw std::runtime_error(
                "K++ IR: expresión sin operando derecho");
        }

        // ----------------------------------------------------
        // Generar operando izquierdo
        // ----------------------------------------------------

        std::string left =
            generateExpression(
                *binary->left,
                function);

        // ----------------------------------------------------
        // Generar operando derecho
        // ----------------------------------------------------

        std::string right =
            generateExpression(
                *binary->right,
                function);

        // ----------------------------------------------------
        // Resultado
        // ----------------------------------------------------

        std::string result =
            createTemporary();

        // ====================================================
        // ARITMÉTICA
        // ====================================================

        if (binary->op == "+")
        {
            function.instructions.push_back({
                IROpcode::Add,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == "-")
        {
            function.instructions.push_back({
                IROpcode::Sub,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == "*")
        {
            function.instructions.push_back({
                IROpcode::Mul,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == "/")
        {
            function.instructions.push_back({
                IROpcode::Div,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == "%")
        {
            function.instructions.push_back({
                IROpcode::Mod,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        // ====================================================
        // POTENCIA
        // ====================================================

        if (binary->op == "^")
        {
            function.instructions.push_back({
                IROpcode::Pow,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        // ====================================================
        // COMPARACIONES
        // ====================================================

        if (binary->op == "==")
        {
            function.instructions.push_back({
                IROpcode::CompareEqual,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == "!=")
        {
            function.instructions.push_back({
                IROpcode::CompareNotEqual,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == "<")
        {
            function.instructions.push_back({
                IROpcode::CompareLess,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == "<=")
        {
            function.instructions.push_back({
                IROpcode::CompareLessEqual,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == ">")
        {
            function.instructions.push_back({
                IROpcode::CompareGreater,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == ">=")
        {
            function.instructions.push_back({
                IROpcode::CompareGreaterEqual,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        // ====================================================
        // LÓGICA
        // ====================================================

        if (binary->op == "&&")
        {
            function.instructions.push_back({
                IROpcode::LogicalAnd,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        if (binary->op == "||")
        {
            function.instructions.push_back({
                IROpcode::LogicalOr,
                "",
                left + ", " + right,
                result,
                0
            });

            return result;
        }

        // ====================================================
        // NOT LÓGICO
        //
        // El parser actual representa:
        //
        // !x
        //
        // como:
        //
        // BinaryExpression("!", 0, x)
        //
        // Por tanto, el resultado se genera usando el
        // operando derecho.
        // ====================================================

        if (binary->op == "!")
        {
            function.instructions.push_back({
                IROpcode::LogicalNot,
                "",
                right,
                result,
                0
            });

            return result;
        }

        // ====================================================
        // OPERADOR NO SOPORTADO
        // ====================================================

        throw std::runtime_error(
            "K++ IR: operador no soportado: " +
            binary->op);
    }

    // ========================================================
    // EXPRESIÓN NO SOPORTADA
    // ========================================================

    throw std::runtime_error(
        "K++ IR: expresión no soportada");
}

// ============================================================

} // namespace kpp
