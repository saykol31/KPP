
#pragma once

#include <string>
#include <vector>

namespace kpp {

// ============================================================
// K++ IR OPCODES
// ============================================================

enum class IROpcode
{
    // ========================================================
    // ESTRUCTURA DEL PROGRAMA
    // ========================================================

    FunctionBegin,
    FunctionEnd,

    // ========================================================
    // CONSTANTES
    // ========================================================

    // CONST 42 -> %0
    Const,

    // ========================================================
    // STRING_CONST "Hola" -> %0
    // ========================================================

    StringConst,

    // ========================================================
    // VARIABLES Y MEMORIA
    // ========================================================

    Declare,
    Store,
    Load,

    // Acceso a elementos de arrays.
    ArrayLoad,
    ArrayStore,

    // ========================================================
    // FILE I/O
    // ========================================================

    FileRead,
    FileWrite,

    // ========================================================
    // ARITMÉTICA BÁSICA
    // ========================================================

    Add,                    // +
    Sub,                    // -
    Mul,                    // *
    Div,                    // /
    Mod,                    // %

    // ========================================================
    // POTENCIAS Y RAÍCES
    // ========================================================

    Pow,                    // a ^ b
    Sqrt,                   // sqrt(a)

    // ========================================================
    // LOGARITMOS Y EXPONENCIALES
    // ========================================================

    Log,                    // logaritmo natural
    Log10,                  // logaritmo base 10
    Exp,                    // e^x

    // ========================================================
    // TRIGONOMETRÍA
    // ========================================================

    Sin,                    // seno
    Cos,                    // coseno
    Tan,                    // tangente
    Cot,                    // cotangente
    Sec,                    // secante
    Csc,                    // cosecante

    // ========================================================
    // TRIGONOMETRÍA INVERSA
    // ========================================================

    Asin,                   // arco seno
    Acos,                   // arco coseno
    Atan,                   // arco tangente

    // ========================================================
    // FUNCIONES MATEMÁTICAS
    // ========================================================

    Abs,                    // valor absoluto
    Floor,                  // piso
    Ceil,                   // techo
    Round,                  // redondeo
    Min,                    // mínimo
    Max,                    // máximo

    // ========================================================
    // MATEMÁTICA AVANZADA
    // ========================================================

    Factorial,              // n!
    Sum,                    // sumatoria
    Integral,               // integral

    // ========================================================
    // FUNCIONES
    // ========================================================

    Call,
    Return,

    // ========================================================
    // SALIDA
    // ========================================================

    Print,
    PrintInteger,
    PrintFloat,

    // ========================================================
    // COMPARACIONES
    // ========================================================

    CompareEqual,           // ==
    CompareNotEqual,        // !=
    CompareLess,            // <
    CompareLessEqual,       // <=
    CompareGreater,         // >
    CompareGreaterEqual,    // >=

    // ========================================================
    // LÓGICA
    // ========================================================

    LogicalAnd,             // &&
    LogicalOr,              // ||
    LogicalNot,             // !

    // ========================================================
    // CONTROL DE FLUJO
    // ========================================================

    Label,
    Jump,
    JumpIfZero,
    JumpIfNotZero
};

// ============================================================
// IR INSTRUCTION
// ============================================================

struct IRInstruction
{
    IROpcode opcode;

    // ========================================================
    // Nombre de variable, función, etiqueta, etc.
    // ========================================================

    std::string name;

    // ========================================================
    // Operando textual.
    //
    // Ejemplos:
    //
    // STORE x <- %0
    // ADD %0, %1 -> %2
    // RETURN %2
    // ========================================================

    std::string operand;

    // ========================================================
    // Resultado producido.
    //
    // Ejemplo:
    //
    // LOAD x -> %0
    // CONST 42 -> %1
    // ========================================================

    std::string result;

    // ========================================================
    // Valor inmediato entero.
    //
    // Ejemplo:
    //
    // CONST 42 -> %0
    // ========================================================

    int value = 0;

    // Operando auxiliar para instrucciones que necesitan
    // transportar un segundo temporal sin sobrecargar
    // el campo operand.
    //
    // FILE_WRITE:
    //   name      = ruta
    //   operand   = identidad del array
    //   auxiliary = temporal con cantidad de bytes
    //   value     = capacidad del array
    //   result    = temporal de retorno
    std::string auxiliary;
};

// ============================================================
// IR FUNCTION
// ============================================================

struct IRFunction
{
    std::string name;

    std::vector<IRInstruction> instructions;
};

// ============================================================
// IR PROGRAM
// ============================================================

struct IRProgram
{
    std::vector<IRFunction> functions;
};

// ============================================================
// OPCODE NAME
// ============================================================

const char* irOpcodeName(IROpcode opcode);

} // namespace kpp
