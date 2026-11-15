#pragma once

#include <string>

namespace kpp {

enum class TokenType
{
    // ============================================================
    // CONTROL
    // ============================================================

    EndOfFile,

    // ============================================================
    // LITERALS / IDENTIFIERS
    // ============================================================

    Identifier,
    Number,
    String,

    // ============================================================
    // TYPES / KEYWORDS
    // ============================================================

    KwInt,
    KwString,
    KwStruct,
    KwEnum,
    KwReturn,
    KwPrint,
    KwIf,
    KwElse,
    KwWhile,

    // ============================================================
    // ASSIGNMENT
    // ============================================================

    Assign,

    // ============================================================
    // ARITHMETIC OPERATORS
    // ============================================================

    Plus,
    Minus,
    Multiply,
    Divide,
    Modulo,

    // ============================================================
    // POWER / ROOT
    // ============================================================

    Power,

    // ============================================================
    // COMPARISON
    // ============================================================

    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,

    // ============================================================
    // LOGICAL
    // ============================================================

    LogicalAnd,
    LogicalOr,
    LogicalNot,

    // ============================================================
    // PARENTHESES / BLOCKS
    // ============================================================

    LParen,
    RParen,
    LBracket,
    RBracket,

    LBrace,
    RBrace,

    // ============================================================
    // SEPARATORS
    // ============================================================

    Comma,
    Semicolon,
    ColonColon,
    Dot,

    // ========================================================
    // KEYWORDS DE LAS AMPLIACIONES
    //
    // Toda palabra clave nueva se añade AL FINAL del enum,
    // nunca en medio. KwVoid = 39 (Ampliación 1, 2026-11-14).
    // KwBreak = 40 (Ampliación 2, 2026-11-14).
    // ========================================================

    KwVoid,
    KwBreak
};

struct Token
{
    TokenType type;
    std::string text;
    int line;
    int column;
};

const char* tokenTypeName(TokenType type);

}
