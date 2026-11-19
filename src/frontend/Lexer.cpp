
#include "kpp/Lexer.hpp"

#include <cctype>
#include <stdexcept>

namespace kpp {

// ============================================================
// TOKEN TYPE NAME
// ============================================================

const char* tokenTypeName(TokenType type)
{
    switch (type)
    {
        // ========================================================
        // CONTROL
        // ========================================================

        case TokenType::EndOfFile:
            return "EOF";

        // ========================================================
        // LITERALS / IDENTIFIERS
        // ========================================================

        case TokenType::Identifier:
            return "Identifier";

        case TokenType::Number:
            return "Number";

        case TokenType::String:
            return "String";

        // ========================================================
        // KEYWORDS
        // ========================================================

        case TokenType::KwInt:
            return "int";

        case TokenType::KwString:
            return "string";

        case TokenType::KwStruct:
            return "struct";

        case TokenType::KwEnum:
            return "enum";

        case TokenType::KwReturn:
            return "return";

        case TokenType::KwPrint:
            return "print";

        case TokenType::KwIf:
            return "if";

        case TokenType::KwElse:
            return "else";

        case TokenType::KwWhile:
            return "while";

        case TokenType::KwVoid:
            return "void";

        case TokenType::KwBreak:
            return "break";

        case TokenType::KwFor:
            return "for";

        // ========================================================
        // ASSIGNMENT
        // ========================================================

        case TokenType::Assign:
            return "=";

        // ========================================================
        // ARITHMETIC
        // ========================================================

        case TokenType::Plus:
            return "+";

        case TokenType::Minus:
            return "-";

        case TokenType::Multiply:
            return "*";

        case TokenType::Divide:
            return "/";

        case TokenType::Modulo:
            return "%";

        // ========================================================
        // POWER
        // ========================================================

        case TokenType::Power:
            return "^";

        // ========================================================
        // COMPARISON
        // ========================================================

        case TokenType::Equal:
            return "==";

        case TokenType::NotEqual:
            return "!=";

        case TokenType::Less:
            return "<";

        case TokenType::LessEqual:
            return "<=";

        case TokenType::Greater:
            return ">";

        case TokenType::GreaterEqual:
            return ">=";

        // ========================================================
        // LOGICAL
        // ========================================================

        case TokenType::LogicalAnd:
            return "&&";

        case TokenType::LogicalOr:
            return "||";

        case TokenType::LogicalNot:
            return "!";

        // ========================================================
        // BITS Y DESPLAZAMIENTOS
        // ========================================================

        case TokenType::BitwiseAnd:
            return "&";

        case TokenType::BitwiseOr:
            return "|";

        case TokenType::ShiftLeft:
            return "<<";

        case TokenType::ShiftRight:
            return ">>";

        // ========================================================
        // PARENTHESES / BLOCKS
        // ========================================================

        case TokenType::LParen:
            return "(";
        case TokenType::RParen:
            return ")";

        case TokenType::LBracket:
            return "[";
        case TokenType::RBracket:
            return "]";

        case TokenType::LBrace:
            return "{";
        case TokenType::RBrace:
            return "}";

        // ========================================================
        // SEPARATORS
        // ========================================================

        case TokenType::Comma:
            return ",";

        case TokenType::Semicolon:
            return ";";

        case TokenType::ColonColon:
            return "::";

        case TokenType::Dot:
            return ".";
    }

    return "Unknown";
}


// ============================================================
// CONSTRUCTOR
// ============================================================

Lexer::Lexer(
    const std::string& source)
    : source(source)
{
}


// ============================================================
// PEEK
// ============================================================

char Lexer::peek() const
{
    if (position >= source.size())
        return '\0';

    return source[position];
}


// ============================================================
// ADVANCE
// ============================================================

char Lexer::advance()
{
    if (position >= source.size())
        return '\0';

    char c = source[position++];

    if (c == '\n')
    {
        ++line;
        column = 1;
    }
    else
    {
        ++column;
    }

    return c;
}


// ============================================================
// SKIP WHITESPACE
// ============================================================

void Lexer::skipWhitespace()
{
    while (std::isspace(
        static_cast<unsigned char>(peek())))
    {
        advance();
    }
}


// ============================================================
// IDENTIFIER / KEYWORD
// ============================================================

Token Lexer::identifier()
{
    const int startLine = line;
    const int startColumn = column;

    std::string text;

    while (
        std::isalnum(
            static_cast<unsigned char>(peek()))
        || peek() == '_')
    {
        text += advance();
    }

    // --------------------------------------------------------
    // KEYWORD: int
    // --------------------------------------------------------

    if (text == "int")
    {
        return {
            TokenType::KwInt,
            text,
            startLine,
            startColumn
        };
    }


    // --------------------------------------------------------
    // KEYWORD: string
    // --------------------------------------------------------

    if (text == "string")
    {
        return {
            TokenType::KwString,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // KEYWORD: struct
    // --------------------------------------------------------

    if (text == "struct")
    {
        return {
            TokenType::KwStruct,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // KEYWORD: enum
    // --------------------------------------------------------

    if (text == "enum")
    {
        return {
            TokenType::KwEnum,
            text,
            startLine,
            startColumn

        };

    }

    // --------------------------------------------------------
    // KEYWORD: return
    // --------------------------------------------------------

    if (text == "return")
    {
        return {
            TokenType::KwReturn,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // KEYWORD: print
    // --------------------------------------------------------

    if (text == "print")
    {
        return {
            TokenType::KwPrint,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // KEYWORD: if
    // --------------------------------------------------------

    if (text == "if")
    {
        return {
            TokenType::KwIf,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // KEYWORD: else
    // --------------------------------------------------------

    if (text == "else")
    {
        return {
            TokenType::KwElse,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // KEYWORD: while
    // --------------------------------------------------------

    if (text == "while")
    {
        return {
            TokenType::KwWhile,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // KEYWORD: void
    // --------------------------------------------------------

    if (text == "void")
    {
        return {
            TokenType::KwVoid,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // KEYWORD: break
    // --------------------------------------------------------

    if (text == "break")
    {
        return {
            TokenType::KwBreak,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // KEYWORD: for
    // --------------------------------------------------------

    if (text == "for")
    {
        return {
            TokenType::KwFor,
            text,
            startLine,
            startColumn
        };
    }

    // --------------------------------------------------------
    // IDENTIFIER
    // --------------------------------------------------------

    return {
        TokenType::Identifier,
        text,
        startLine,
        startColumn
    };
}


// ============================================================
// NUMBER
// ============================================================

Token Lexer::number()
{
    const int startLine = line;
    const int startColumn = column;

    std::string text;

    // --------------------------------------------------------
    // Literal hexadecimal: 0x...
    //
    // Ampliación 4: el valor se calcula aquí, en el lexing
    // (contrato de tokens), y el Number nace con su texto
    // ya en decimal, como cualquier otro número: aguas
    // abajo nadie distingue un literal hex de uno decimal.
    // --------------------------------------------------------

    if (peek() == '0' &&
        (source[position + 1] == 'x' ||
         source[position + 1] == 'X'))
    {
        advance(); // '0'
        advance(); // 'x'

        long long value = 0;
        bool anyDigit = false;

        while (std::isxdigit(
            static_cast<unsigned char>(peek())))
        {
            const char digit = advance();
            anyDigit = true;

            if (digit >= '0' && digit <= '9')
                value = value * 16 + (digit - '0');
            else if (digit >= 'a' && digit <= 'f')
                value = value * 16 + (digit - 'a' + 10);
            else
                value = value * 16 + (digit - 'A' + 10);
        }

        if (!anyDigit)
        {
            throw std::runtime_error(
                "K++: se esperaba al menos un dígito "
                "hexadecimal después de '0x'");
        }

        text = std::to_string(value);

        return {
            TokenType::Number,
            text,
            startLine,
            startColumn
        };
    }

    while (
        std::isdigit(
            static_cast<unsigned char>(peek())))
    {
        text += advance();
    }

    return {
        TokenType::Number,
        text,
        startLine,
        startColumn
    };
}


// ============================================================
// STRING
// ============================================================

Token Lexer::stringLiteral()
{
    const int startLine = line;
    const int startColumn = column;

    std::string text;

    // --------------------------------------------------------
    // consumir "
    // --------------------------------------------------------

    advance();

    while (
        peek() != '"'
        && peek() != '\0')
    {
        text += advance();
    }

    // --------------------------------------------------------
    // consumir "
    // --------------------------------------------------------

    if (peek() == '"')
        advance();

    return {
        TokenType::String,
        text,
        startLine,
        startColumn
    };
}


// ============================================================
// TOKENIZE
// ============================================================

std::vector<Token>
Lexer::tokenize()
{
    std::vector<Token> tokens;

    while (true)
    {
        skipWhitespace();

        const int tokenLine = line;
        const int tokenColumn = column;

        char c = peek();

        // ====================================================
        // EOF
        // ====================================================

        if (c == '\0')
        {
            tokens.push_back({
                TokenType::EndOfFile,
                "",
                tokenLine,
                tokenColumn
            });

            break;
        }


        // ====================================================
        // STRING
        // ====================================================

        if (c == '"')
        {
            tokens.push_back(
                stringLiteral());

            continue;
        }


        // ====================================================
        // IDENTIFIER / KEYWORD
        // ====================================================

        if (
            std::isalpha(
                static_cast<unsigned char>(c))
            || c == '_')
        {
            tokens.push_back(
                identifier());

            continue;
        }


        // ====================================================
        // NUMBER
        // ====================================================

        if (
            std::isdigit(
                static_cast<unsigned char>(c)))
        {
            tokens.push_back(
                number());

            continue;
        }


        // ====================================================
        // ::
        // ====================================================

        if (c == ':' &&
            position + 1 < source.size() &&
            source[position + 1] == ':')
        {
            advance();
            advance();

            tokens.push_back({
                TokenType::ColonColon,
                "::",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // +
        // ====================================================

        if (c == '+')
        {
            advance();

            tokens.push_back({
                TokenType::Plus,
                "+",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // -
        // ====================================================

        if (c == '-')
        {
            advance();

            tokens.push_back({
                TokenType::Minus,
                "-",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // *
        // ====================================================

        if (c == '*')
        {
            advance();

            tokens.push_back({
                TokenType::Multiply,
                "*",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // /  o  // comentario de línea
        // ====================================================

        if (c == '/')
        {
            advance();

            // // = comentario hasta fin de línea (sin token)
            if (peek() == '/')
            {
                while (
                    peek() != '\n'
                    && peek() != '\0')
                {
                    advance();
                }

                continue;
            }

            tokens.push_back({
                TokenType::Divide,
                "/",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // %
        // ====================================================

        if (c == '%')
        {
            advance();

            tokens.push_back({
                TokenType::Modulo,
                "%",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // ^
        // ====================================================

        if (c == '^')
        {
            advance();

            tokens.push_back({
                TokenType::Power,
                "^",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // =
        // ==
        // ====================================================

        if (c == '=')
        {
            advance();

            if (peek() == '=')
            {
                advance();

                tokens.push_back({
                    TokenType::Equal,
                    "==",
                    tokenLine,
                    tokenColumn
                });
            }
            else
            {
                tokens.push_back({
                    TokenType::Assign,
                    "=",
                    tokenLine,
                    tokenColumn
                });
            }

            continue;
        }


        // ====================================================
        // !
        // !=
        // ====================================================

        if (c == '!')
        {
            advance();

            if (peek() == '=')
            {
                advance();

                tokens.push_back({
                    TokenType::NotEqual,
                    "!=",
                    tokenLine,
                    tokenColumn
                });
            }
            else
            {
                tokens.push_back({
                    TokenType::LogicalNot,
                    "!",
                    tokenLine,
                    tokenColumn
                });
            }

            continue;
        }


        // ====================================================
        // <
        // <=
        // <<
        // ====================================================

        if (c == '<')
        {
            advance();

            if (peek() == '=')
            {
                advance();

                tokens.push_back({
                    TokenType::LessEqual,
                    "<=",
                    tokenLine,
                    tokenColumn
                });
            }
            else if (peek() == '<')
            {
                advance();

                tokens.push_back({
                    TokenType::ShiftLeft,
                    "<<",
                    tokenLine,
                    tokenColumn
                });
            }
            else
            {
                tokens.push_back({
                    TokenType::Less,
                    "<",
                    tokenLine,
                    tokenColumn
                });
            }

            continue;
        }


        // ====================================================
        // >
        // >=
        // >>
        // ====================================================

        if (c == '>')
        {
            advance();

            if (peek() == '=')
            {
                advance();

                tokens.push_back({
                    TokenType::GreaterEqual,
                    ">=",
                    tokenLine,
                    tokenColumn
                });
            }
            else if (peek() == '>')
            {
                advance();

                tokens.push_back({
                    TokenType::ShiftRight,
                    ">>",
                    tokenLine,
                    tokenColumn
                });
            }
            else
            {
                tokens.push_back({
                    TokenType::Greater,
                    ">",
                    tokenLine,
                    tokenColumn
                });
            }

            continue;
        }


        // ====================================================
        // &&
        // &
        // ====================================================

        if (c == '&')
        {
            advance();

            if (peek() == '&')
            {
                advance();

                tokens.push_back({
                    TokenType::LogicalAnd,
                    "&&",
                    tokenLine,
                    tokenColumn
                });

                continue;
            }

            // Ampliación 4: el & solitario es el AND de bits.
            tokens.push_back({
                TokenType::BitwiseAnd,
                "&",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // ||
        // |
        // ====================================================

        if (c == '|')
        {
            advance();

            if (peek() == '|')
            {
                advance();

                tokens.push_back({
                    TokenType::LogicalOr,
                    "||",
                    tokenLine,
                    tokenColumn
                });

                continue;
            }

            // Ampliación 4: el | solitario es el OR de bits.
            tokens.push_back({
                TokenType::BitwiseOr,
                "|",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // (
        // ====================================================

        if (c == '(')
        {
            advance();

            tokens.push_back({
                TokenType::LParen,
                "(",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // )
        // ====================================================

        if (c == ')')
        {
            advance();

            tokens.push_back({
                TokenType::RParen,
                ")",
                tokenLine,
                tokenColumn
            });

            continue;
        }

        // ====================================================
        // [
        // ====================================================

        if (c == '[')
        {
            advance();

            tokens.push_back({
                TokenType::LBracket,
                "[",
                tokenLine,
                tokenColumn
            });

            continue;
        }

        // ====================================================
        // ]
        // ====================================================

        if (c == ']')
        {
            advance();

            tokens.push_back({
                TokenType::RBracket,
                "]",
                tokenLine,
                tokenColumn
            });

            continue;
        }
        // ====================================================
        // {
        // ====================================================

        if (c == '{')
        {
            advance();

            tokens.push_back({
                TokenType::LBrace,
                "{",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // }
        // ====================================================

        if (c == '}')
        {
            advance();

            tokens.push_back({
                TokenType::RBrace,
                "}",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // ,
        // ====================================================

        if (c == ',')
        {
            advance();

            tokens.push_back({
                TokenType::Comma,
                ",",
                tokenLine,
                tokenColumn
            });

            continue;
        }


        // ====================================================
        // ;
        // ====================================================

        if (c == ';')
        {
            advance();

            tokens.push_back({
                TokenType::Semicolon,
                ";",
                tokenLine,
                tokenColumn
            });

            continue;
        }

        // ====================================================
        // .
        // ====================================================

        if (c == '.')
        {
            advance();

            tokens.push_back({
                TokenType::Dot,
                ".",
                tokenLine,
                tokenColumn
            });

            continue;
        }

        // ====================================================
        // CARACTER DESCONOCIDO
        // ====================================================

        throw std::runtime_error(
            std::string("K++: carácter no reconocido: '")
            + c
            + "'");
    }

    return tokens;
}

}
