
#include "kpp/Lexer.hpp"
#include "kpp/Parser.hpp"
#include "kpp/SemanticAnalyzer.hpp"
#include "kpp/IRGenerator.hpp"
#include "kpp/X86Backend.hpp"
#include "kpp/BinaryWriter.hpp"
#include "kpp/KolibriExecutableWriter.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr
            << "K++ Compiler 0.4\n"
            << "Uso: kpp archivo.cpp\n";

        return 1;
    }

    std::ifstream file(argv[1]);

    if (!file)
    {
        std::cerr
            << "K++: no se pudo abrir: "
            << argv[1]
            << '\n';

        return 1;
    }

    std::stringstream buffer;

    buffer << file.rdbuf();

    // ========================================================
    // LEXER
    // ========================================================

    kpp::Lexer lexer(buffer.str());

    auto tokens = lexer.tokenize();

    // ========================================================
    // PARSER
    // ========================================================

    try
    {
        kpp::Parser parser(tokens);

        auto program = parser.parse();


        // ====================================================
        // AST
        // ====================================================

        std::cout
            << "K++ COMPILER 0.4\n"
            << "================\n\n";

        std::cout
            << "AST\n"
            << "---\n";

               std::cout << "Program\n";

        // ====================================================
        // STRUCT DECLARATIONS
        // ====================================================

        for (const auto& structure : program->structs)
        {
            std::cout
                << "├── Struct: "
                << structure->name
                << '\n';

            for (std::size_t i = 0;
                 i < structure->fields.size();
                 ++i)
            {
                const auto& field = structure->fields[i];
                const bool last = (i + 1 == structure->fields.size());

                std::cout
                    << "│   "
                    << (last ? "└── " : "├── ")
                    << field.type
                    << " "
                    << field.name
                    << '\n';
            }
        }

        // ====================================================
        // FUNCTION DECLARATIONS
        // ====================================================

        for (const auto& function : program->functions)
        {
            std::cout
                << "└── Function: "
                << function->name
                << '\n';

            std::cout
                << "    ├── Return type: "
                << function->returnType
                << '\n';

            // ==================================================
            // PARAMETERS
            // ==================================================

            if (!function->parameters.empty())
            {
                std::cout
                    << "    ├── Parameters\n";

                for (std::size_t i = 0;
                     i < function->parameters.size();
                     ++i)
                {
                    const auto& parameter =
                        function->parameters[i];

                    const bool last =
                        (i + 1 == function->parameters.size());

                    std::cout
                        << "    │   "
                        << (last ? "└── " : "├── ")
                        << parameter.type
                        << " "
                        << parameter.name
                        << '\n';
                }
            }

            std::cout
                << "    └── Body\n";

            for (const auto& statement :
                function->body->statements)
            {
                // ==================================================
                // VARIABLE DECLARATION
                // ==================================================

                if (auto* variable =
                    dynamic_cast<
                        kpp::VariableDeclaration*>(
                        statement.get()))
                {
                    std::cout
                        << "        ├── Variable: "
                        << variable->name
                        << '\n';

                    if (auto* integer =
                        dynamic_cast<
                            kpp::IntegerLiteral*>(
                            variable->initializer.get()))
                    {
                        std::cout
                            << "        │   └── Integer: "
                            << integer->value
                            << '\n';
                    }
                    else if (auto* expression =
                        dynamic_cast<
                            kpp::VariableExpression*>(
                            variable->initializer.get()))
                    {
                        std::cout
                            << "        │   └── Variable: "
                            << expression->name
                            << '\n';
                    }

                    continue;
                }

                // ==================================================
                // PRINT
                // ==================================================

                if (auto* print =
                    dynamic_cast<
                        kpp::PrintStatement*>(
                        statement.get()))
                {
                    std::cout
                        << "        ├── Print"
                        << '\n';

                    if (auto* integer =
                        dynamic_cast<
                            kpp::IntegerLiteral*>(
                            print->expression.get()))
                    {
                        std::cout
                            << "        │   └── Integer: "
                            << integer->value
                            << '\n';
                    }
                    else if (auto* string =
                        dynamic_cast<
                            kpp::StringLiteral*>(
                            print->expression.get()))
                    {
                        std::cout
                            << "        │   └── String: "
                            << string->value
                            << '\n';
                    }
                    else if (auto* variable =
                        dynamic_cast<
                            kpp::VariableExpression*>(
                            print->expression.get()))
                    {
                        std::cout
                            << "        │   └── Variable: "
                            << variable->name
                            << '\n';
                    }

                    continue;
                }

                // ==================================================
                // WHILE
                // ==================================================

                if (auto* whileStatement =
                    dynamic_cast<
                        kpp::WhileStatement*>(
                        statement.get()))
                {
                    std::cout
                        << "        ├── While\n";

                    // --------------------------------------------------
                    // CONDITION
                    // --------------------------------------------------

                    std::cout
                        << "        │   ├── Condition\n";

                    if (auto* integer =
                        dynamic_cast<
                            kpp::IntegerLiteral*>(
                            whileStatement->condition.get()))
                    {
                        std::cout
                            << "        │   │   └── Integer: "
                            << integer->value
                            << '\n';
                    }
                    else if (auto* variable =
                        dynamic_cast<
                            kpp::VariableExpression*>(
                            whileStatement->condition.get()))
                    {
                        std::cout
                            << "        │   │   └── Variable: "
                            << variable->name
                            << '\n';
                    }

                    // --------------------------------------------------
                    // BODY
                    // --------------------------------------------------

                    std::cout
                        << "        │   └── Body\n";

                    if (whileStatement->body)
                    {
                        for (const auto& nestedStatement :
                            whileStatement->body->statements)
                        {
                            if (auto* print =
                                dynamic_cast<
                                    kpp::PrintStatement*>(
                                    nestedStatement.get()))
                            {
                                std::cout
                                    << "        │     └── Print"
                                    << '\n';

                                if (auto* integer =
                                    dynamic_cast<
                                        kpp::IntegerLiteral*>(
                                        print->expression.get()))
                                {
                                    std::cout
                                        << "        │         └── Integer: "
                                        << integer->value
                                        << '\n';
                                }
                                else if (auto* string =
                                    dynamic_cast<
                                        kpp::StringLiteral*>(
                                        print->expression.get()))
                                {
                                    std::cout
                                        << "        │         └── String: "
                                        << string->value
                                        << '\n';
                                }
                                else if (auto* variable =
                                    dynamic_cast<
                                        kpp::VariableExpression*>(
                                        print->expression.get()))
                                {
                                    std::cout
                                        << "        │         └── Variable: "
                                        << variable->name
                                        << '\n';
                                }
                            }
                        }
                    }

                    continue;
                }


                // ==================================================
                // RETURN
                // ==================================================

                if (auto* ret =
                    dynamic_cast<
                        kpp::ReturnStatement*>(
                        statement.get()))
                {
                    std::cout
                        << "        └── Return\n";

                    if (auto* integer =
                        dynamic_cast<
                            kpp::IntegerLiteral*>(
                            ret->expression.get()))
                    {
                        std::cout
                            << "            └── Integer: "
                            << integer->value
                            << '\n';
                    }
                    else if (auto* variable =
                        dynamic_cast<
                            kpp::VariableExpression*>(
                            ret->expression.get()))
                    {
                        std::cout
                            << "            └── Variable: "
                            << variable->name
                            << '\n';
                    }
                    else if (auto* call =
                        dynamic_cast<
                            kpp::CallExpression*>(
                            ret->expression.get()))
                    {
                        std::cout
                            << "            └── Call: "
                            << call->callee
                            << '\n';

                        if (!call->arguments.empty())
                    {
                        std::cout
                            << "                └── Arguments\n";

                        auto printArgument =
                            [&](const auto& self,
                                const kpp::Expression* expression,
                                const std::string& prefix,
                                bool last) -> void
                        {
                            const std::string branch =
                                last
                                    ? "└── "
                                    : "├── ";

                            if (auto* integer =
                                    dynamic_cast<
                                        const kpp::IntegerLiteral*>(
                                        expression))
                            {
                                std::cout
                                    << prefix
                                    << branch
                                    << "Integer: "
                                    << integer->value
                                    << '\n';

                                return;
                            }

                            if (auto* variable =
                                    dynamic_cast<
                                        const kpp::VariableExpression*>(
                                        expression))
                            {
                                std::cout
                                    << prefix
                                    << branch
                                    << "Variable: "
                                    << variable->name
                                    << '\n';

                                return;
                            }

                            if (auto* binary =
                                    dynamic_cast<
                                        const kpp::BinaryExpression*>(
                                        expression))
                            {
                                std::cout
                                    << prefix
                                    << branch
                                    << "Binary: "
                                    << binary->op
                                    << '\n';

                                const std::string childPrefix =
                                    prefix +
                                    (last
                                        ? "    "
                                        : "│   ");

                                if (binary->left)
                                {
                                    self(
                                        self,
                                        binary->left.get(),
                                        childPrefix,
                                        !binary->right);
                                }

                                if (binary->right)
                                {
                                    self(
                                        self,
                                        binary->right.get(),
                                        childPrefix,
                                        true);
                                }

                                return;
                            }

                            std::cout
                                << prefix
                                << branch
                                << "Expression\n";
                        };

                        for (std::size_t i = 0;
                             i < call->arguments.size();
                             ++i)
                        {
                            const auto& argument =
                                call->arguments[i];

                            const bool last =
                                (i + 1 ==
                                 call->arguments.size());

                            printArgument(
                                printArgument,
                                argument.get(),
                                "                    ",
                                last);
                        }
                    }
                    }
                }
            }
        }

        // ====================================================
        // SEMANTIC ANALYZER
        // ====================================================

        std::cout
            << "\nSEMANTIC ANALYZER\n"
            << "=================\n";

        kpp::SemanticAnalyzer analyzer;

        if (!analyzer.analyze(*program))
        {
            std::cout
                << "\n✘ Errores semánticos:\n\n";

            for (const auto& error :
                 analyzer.errors())
            {
                std::cout
                    << "  error: "
                    << error.message
                    << '\n';
            }

            return 1;
        }

        std::cout
            << "✔ Programa semánticamente válido\n";

        std::cout
            << "✔ main() encontrada\n";

        std::cout
            << "✔ Tipos válidos\n";

        std::cout
            << "✔ Expresiones válidas\n";

        std::cout
            << "✔ Sentencias válidas\n";

        std::cout
            << "\n✔ K++ frontend OK\n";

        // ========================================================
        // K++ IR
        // ========================================================

        std::cout
            << "\nK++ IR\n"
            << "======\n";

        kpp::IRGenerator irGenerator;

        auto ir =
            irGenerator.generate(*program);

        for (const auto& function :
             ir.functions)
        {
            for (const auto& instruction :
                 function.instructions)
            {

                switch (instruction.opcode)
                {
                    // ========================================================
                    // FUNCTION BEGIN
                    // ========================================================

                    case kpp::IROpcode::FunctionBegin:

                        std::cout
                            << "FUNCTION "
                            << instruction.name;

                        if (!instruction.operand.empty())
                        {
                            std::cout
                                << " "
                                << instruction.operand;
                        }

                        std::cout
                            << '\n';

                        break;


                    // ========================================================
                    // CONSTANT
                    // ========================================================

                    case kpp::IROpcode::Const:

                        std::cout
                            << "CONST "
                            << instruction.value
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // STRING CONSTANT
                    // ========================================================

                    case kpp::IROpcode::StringConst:

                        std::cout
                            << "STRING_CONST "
                            << instruction.name
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // DECLARE
                    // ========================================================

                    case kpp::IROpcode::Declare:

                        std::cout
                            << "DECLARE "
                            << instruction.name
                            << '\n';

                        break;


                    // ========================================================
                    // STORE
                    // ========================================================

                    case kpp::IROpcode::Store:

                        std::cout
                            << "STORE "
                            << instruction.name;

                        if (!instruction.operand.empty())
                        {
                            std::cout
                                << " <- "
                                << instruction.operand;
                        }
                        else
                        {
                            std::cout
                                << " <- "
                                << instruction.value;
                        }

                        std::cout << '\n';

                        break;


                    // ========================================================
                    // LOAD
                    // ========================================================

                    case kpp::IROpcode::Load:

                        std::cout
                            << "LOAD "
                            << instruction.name;

                        if (!instruction.result.empty())
                        {
                            std::cout
                                << " -> "
                                << instruction.result;
                        }

                        std::cout << '\n';

                        break;


                    // ========================================================
                    // ARITHMETIC
                    // ========================================================

                    case kpp::IROpcode::Add:

                        std::cout
                            << "ADD "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Sub:

                        std::cout
                            << "SUB "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Mul:

                        std::cout
                            << "MUL "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Div:

                        std::cout
                            << "DIV "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Mod:

                        std::cout
                            << "MOD "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // POWERS / ROOTS
                    // ========================================================

                    case kpp::IROpcode::Pow:

                        std::cout
                            << "POW "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Sqrt:

                        std::cout
                            << "SQRT "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // LOGARITHMS / EXPONENTIAL
                    // ========================================================

                    case kpp::IROpcode::Log:

                        std::cout
                            << "LOG "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Log10:

                        std::cout
                            << "LOG10 "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Exp:

                        std::cout
                            << "EXP "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // TRIGONOMETRY
                    // ========================================================

                    case kpp::IROpcode::Sin:

                        std::cout
                            << "SIN "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Cos:

                        std::cout
                            << "COS "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Tan:

                        std::cout
                            << "TAN "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Cot:

                        std::cout
                            << "COT "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Sec:

                        std::cout
                            << "SEC "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Csc:

                        std::cout
                            << "CSC "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // INVERSE TRIGONOMETRY
                    // ========================================================

                    case kpp::IROpcode::Asin:

                        std::cout
                            << "ASIN "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Acos:

                        std::cout
                            << "ACOS "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Atan:

                        std::cout
                            << "ATAN "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // MATH FUNCTIONS
                    // ========================================================

                    case kpp::IROpcode::Abs:

                        std::cout
                            << "ABS "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Floor:

                        std::cout
                            << "FLOOR "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Ceil:

                        std::cout
                            << "CEIL "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Round:

                        std::cout
                            << "ROUND "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Min:

                        std::cout
                            << "MIN "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Max:

                        std::cout
                            << "MAX "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // ADVANCED MATH
                    // ========================================================

                    case kpp::IROpcode::Factorial:

                        std::cout
                            << "FACTORIAL "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Sum:

                        std::cout
                            << "SUM "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::Integral:

                        std::cout
                            << "INTEGRAL "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // FUNCTIONS
                    // ========================================================

                    case kpp::IROpcode::Call:

                        std::cout
                            << "CALL "
                            << instruction.name;

                        if (!instruction.operand.empty())
                        {
                            std::cout
                                << " "
                                << instruction.operand;
                        }

                        if (!instruction.result.empty())
                        {
                            std::cout
                                << " -> "
                                << instruction.result;
                        }

                        std::cout << '\n';

                        break;


                    case kpp::IROpcode::Return:

                        std::cout
                            << "RETURN ";

                        if (!instruction.operand.empty())
                        {
                            std::cout
                                << instruction.operand;
                        }
                        else
                        {
                            std::cout
                                << instruction.value;
                        }

                        std::cout << '\n';

                        break;


                    // ========================================================
                    // OUTPUT
                    // ========================================================

                    case kpp::IROpcode::Print:

                        std::cout
                            << "PRINT "
                            << instruction.name
                            << '\n';

                        break;


                    case kpp::IROpcode::PrintInteger:

                        std::cout
                            << "PRINT_INT "
                            << instruction.operand
                            << '\n';

                        break;


                    case kpp::IROpcode::PrintFloat:

                        std::cout
                            << "PRINT_FLOAT "
                            << instruction.operand
                            << '\n';

                        break;


                    // ========================================================
                    // COMPARISONS
                    // ========================================================

                    case kpp::IROpcode::CompareEqual:

                        std::cout
                            << "CMP_EQ "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::CompareNotEqual:

                        std::cout
                            << "CMP_NE "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::CompareLess:

                        std::cout
                            << "CMP_LT "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::CompareLessEqual:

                        std::cout
                            << "CMP_LE "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::CompareGreater:

                        std::cout
                            << "CMP_GT "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::CompareGreaterEqual:

                        std::cout
                            << "CMP_GE "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // LOGIC
                    // ========================================================

                    case kpp::IROpcode::LogicalAnd:

                        std::cout
                            << "AND "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::LogicalOr:

                        std::cout
                            << "OR "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    case kpp::IROpcode::LogicalNot:

                        std::cout
                            << "NOT "
                            << instruction.operand
                            << " -> "
                            << instruction.result
                            << '\n';

                        break;


                    // ========================================================
                    // CONTROL FLOW
                    // ========================================================

                    case kpp::IROpcode::Label:

                        std::cout
                            << "LABEL "
                            << instruction.name
                            << '\n';

                        break;


                    case kpp::IROpcode::Jump:

                        std::cout
                            << "JUMP "
                            << instruction.name
                            << '\n';

                        break;


                    case kpp::IROpcode::JumpIfZero:

                        std::cout
                            << "JUMP_IF_ZERO "
                            << instruction.operand
                            << " -> "
                            << instruction.name
                            << '\n';

                        break;


                    case kpp::IROpcode::JumpIfNotZero:

                        std::cout
                            << "JUMP_IF_NOT_ZERO "
                            << instruction.operand
                            << " -> "
                            << instruction.name
                            << '\n';

                        break;


                    // ========================================================
                    // FUNCTION END
                    // ========================================================

                    case kpp::IROpcode::FunctionEnd:

                        std::cout
                            << "END\n";

                        break;
                }

            }
        }

        std::cout
            << "\n✔ K++ IR generada correctamente\n";


        // ========================================================
        // X86 BACKEND
        // ========================================================

        std::cout
            << "\nK++ x86 BACKEND\n"
            << "===============\n";

        kpp::X86Backend x86Backend;

        auto machineCode =
            x86Backend.generate(ir);

        std::cout
            << "Machine code:\n\n";

        for (std::size_t i = 0;
            i < machineCode.size();
            ++i)
        {
            std::printf(
                "%02X ",
                machineCode[i]);

            if ((i + 1) % 16 == 0)
                std::printf("\n");
        }

        if (machineCode.size() % 16 != 0)
            std::printf("\n");

        std::cout
            << "\nBytes generados: "
            << machineCode.size()
            << '\n';

        std::cout
            << "\n✔ K++ x86 backend OK\n";


        // ========================================================
        // BINARY WRITER
        // ========================================================

        std::cout
            << "\nK++ BINARY WRITER\n"
            << "=================\n";

        // ========================================================
        // OUTPUT POR ARCHIVO FUENTE
        // ========================================================

        const std::filesystem::path sourcePath(argv[1]);

        const std::string programName =
            sourcePath.stem().string();

        const std::filesystem::path outputDir =
            "Output";

        std::filesystem::create_directories(outputDir);

        const std::string outputFile =
            (outputDir / (programName + ".raw")).string();

        const std::string kolibriOutput =
            (outputDir / (programName + ".bin")).string();

        std::cout
            << "K++ OUTPUT\n"
            << "==========\n"
            << "Fuente: " << sourcePath.string() << '\n'
            << "Programa: " << programName << '\n'
            << "RAW: " << outputFile << '\n'
            << "BIN: " << kolibriOutput << '\n';

        kpp::BinaryWriter binaryWriter;

        if (!binaryWriter.write(
                outputFile,
                machineCode))
        {
            std::cerr
                << "✘ K++: no se pudo escribir: "
                << outputFile
                << '\n';

            return 1;
        }

        std::cout
            << "Archivo generado: "
            << outputFile
            << '\n';

        std::cout
            << "Tamaño: "
            << machineCode.size()
            << " bytes\n";

        std::cout
            << "\n✔ K++ Binary Writer OK\n";

        // ========================================================
        // KOLIBRIOS EXECUTABLE WRITER
        // ========================================================

        std::cout
            << "\nK++ KOLIBRIOS EXECUTABLE WRITER\n"
            << "================================\n";

        kpp::KolibriExecutableWriter
            kolibriWriter;

        if (!kolibriWriter.write(
                kolibriOutput,
                machineCode))
        {
            std::cerr
                << "✘ K++: no se pudo generar: "
                << kolibriOutput
                << '\n';

            return 1;
        }

        std::cout
            << "Ejecutable generado: "
            << kolibriOutput
            << '\n';

        std::cout
            << "Código x86: "
            << machineCode.size()
            << " bytes\n";

        std::cout
            << "✔ K++ KolibriOS executable OK\n";


    }
    catch (const std::exception& e)
    {
        std::cerr
            << "\n✘ Error: "
            << e.what()
            << '\n';

        return 1;
    }

    return 0;
}
