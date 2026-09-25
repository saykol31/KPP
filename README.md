K++ 0.9

tree

KPP
│
├── CMakeLists.txt
├── README.md
├── kpp.cpp
│
├── include/kpp/
│   ├── Token.hpp
│   ├── Lexer.hpp
│   ├── AST.hpp
│   ├── Parser.hpp
│   ├── SemanticAnalyzer.hpp
│   │
│   ├── IR.hpp
│   ├── IRGenerator.hpp
│   │
│   ├── X86Backend.hpp
│   ├── X86Emitter.hpp
│   │
│   ├── BinaryWriter.hpp
│   ├── KolibriExecutableWriter.hpp
│   └── KolibriRuntime.hpp
│
├── src/
│   ├── main.cpp
│   │
│   ├── frontend/
│   │   ├── Lexer.cpp
│   │   ├── Parser.cpp
│   │   └── SemanticAnalyzer.cpp
│   │
│   ├── backend/
│   │   ├── IRGenerator.cpp
│   │   ├── X86Backend.cpp
│   │   ├── X86Emitter.cpp
│   │   ├── BinaryWriter.cpp
│   │   └── KolibriExecutableWriter.cpp
│   │
│   └── runtime/
│       └── KolibriRuntime.cpp
│
├── projects/
│   ├── hello.cpp
│   ├── arithmetic.cpp
│   ├── precedence.cpp
│   ├── variable.cpp
│   ├── temporary.cpp
│   ├── temporary2.cpp
│   ├── print.cpp
│   ├── print3.cpp
│   ├── allocator_stress.cpp
│   ├── error.cpp
│   └── variable_error.cpp
│
└── temporal/


objetivo

FASE 1
┌──────────────────────────────┐
│ Variables + asignación       │
│ scopes                       │
└──────────────┬───────────────┘
               ↓
FASE 2
┌──────────────────────────────┐
│ if / else                    │
│ comparación                  │
│ saltos                       │
└──────────────┬───────────────┘
               ↓
FASE 3
┌──────────────────────────────┐
│ while                        │
│ loops                        │
└──────────────┬───────────────┘
               ↓
FASE 4
┌──────────────────────────────┐
│ funciones                    │
│ parámetros                   │
│ llamadas                     │
└──────────────┬───────────────┘
               ↓
FASE 5
┌──────────────────────────────┐
│ memoria / arrays / punteros  │
└──────────────┬───────────────┘
               ↓
FASE 6
┌──────────────────────────────┐
│ runtime K++                  │
│ I/O                          │
│ strings                      │
└──────────────┬───────────────┘
               ↓
FASE 7
┌──────────────────────────────┐
│ escribir KPP en K++          │
└──────────────┬───────────────┘
               ↓
FASE 8
┌──────────────────────────────┐
│ K++ compila KPP              │
│                              │
│ SELF-HOSTING                 │
└──────────────────────────────┘


lo que tenemos


| Componente          | Estado                                   |
| ------------------- | ---------------------------------------- |
| Lexer               | 🟡 funcional, faltan keywords de control |
| Parser              | 🟡 necesita crecer junto al AST          |
| AST                 | 🟡 falta control de flujo/funciones      |
| SemanticAnalyzer    | 🟡 necesita scopes y nuevas sentencias   |
| IR                  | 🟢 ya tiene bastante infraestructura     |
| IRGenerator         | 🟡 necesita conectar el IR existente     |
| Allocator           | 🟢 funcional para el conjunto actual     |
| X86Emitter          | 🟢 tiene saltos básicos y patching       |
| X86Backend          | 🟡 falta conectar control-flow           |
| KolibriRuntime      | 🟢 funcional como base                   |
| BinaryWriter        | 🟢 probado                               |
| Ejecución KolibriOS | 🟢 probada                               |
| Self-hosting        | 🔴 todavía falta lenguaje suficiente     |



Tipos
 ├── int
 ├── void
 └── quizá char

Variables
 ├── declaración
 ├── asignación
 └── alcance

Expresiones
 ├── aritmética
 ├── comparaciones
 └── llamadas

Control
 ├── if
 ├── else
 └── while

Funciones
 ├── parámetros
 ├── return
 └── call

Infraestructura
 ├── arrays/punteros o equivalente mínimo
 ├── acceso a memoria
 └── primitivas/runtime



                 K++ 0.4
                    │
                    ▼
             VARIABLES ✔
                    │
             EXPRESIONES ✔
                    │
             OPERADORES ✔
                    │
              LOAD/STORE ✔
                    │
                RETURN ✔
                    │
              ALLOCATOR ✔
                    │
          ──────────┴──────────
                    │
                    ▼
             COMPARACIONES
                    │
                    ▼
                   IF
                    │
                    ▼
                IF / ELSE
                    │
                    ▼
                  WHILE
                    │
                    ▼
              FUNCIONES
                    │
                    ▼
             PARAMETROS/CALL
                    │
                    ▼
             ARRAYS / MEMORIA
                    │
                    ▼
              K++ MINIMO
                    │
                    ▼
          COMPILADOR ESCRITO
             EN K++
                    │
                    ▼
             SELF-HOSTING
                    │
                    ▼
              COMUNIDAD
                    │
          ┌─────────┼─────────┐
          ▼         ▼         ▼
        ARM       x86-64    nuevas
                             features
==========================

K++ is an experimental C++ compiler being developed for KolibriOS.

The goal of K++ is to create a small, independent C++ compiler capable of translating C++ source code into native x86 KolibriOS executables.

K++ is being developed as a compiler project from the ground up, with its own frontend, intermediate representation, x86 backend and KolibriOS executable writer.

The project is designed to progressively evolve from a minimal C++ compiler into a complete development toolchain for KolibriOS.

Compiler Pipeline

K++ processes a C++ program through several compilation stages:

                         K++ 2.9
                            │
                  ┌─────────┴─────────┐
                  │                   │
                LEXER               PARSER
                  │                   │
                  ▼                   ▼
                TOKENS               AST               AST
                                                        │
                                                        ▼
                                              generateExpression()
                                                        │
                                                ┌───────┴────────┐
                                                │                │
                                             literal          variable
                                                │                │
                                                ▼                ▼
                                             CONSTANT           LOAD
                                                │                │
                                                └───────┬────────┘
                                                        ▼
                                                   operación
                                                        │
                                                        ▼
                                                   resultado %N
                  │                   │
                  └─────────┬─────────┘
                            ▼
                   SEMANTIC ANALYZER
                            │
                            ▼
                 K++ IR (Intermediate                        │
          ┌─────────────┼─────────────┐
          ▼             ▼             ▼
       CONST          ARITH         LOGIC
          │             │             │
          │       + - * / % ^      == != < >
          │                       <= >= && ||
          │
          ▼
                    Representation)
                            │
                            ▼
                       x86 BACKEND
                            │
                            ▼
                    BINARY WRITER
                            │
                            ▼
                 KOLIBRIOS EXECUTABLE
                       WRITER
                            │
                            ▼
                       KOLIBRIOS BIN

The compiler is divided into independent stages so that each part can be developed and tested separately.

Frontend

The K++ frontend is responsible for understanding the C++ source code.

Lexer

The lexer reads the source code and converts it into tokens.

Examples include:

identifiers
numbers
strings
keywords
operators
parentheses
braces
semicolons
Parser

The parser consumes the tokens and builds an Abstract Syntax Tree (AST) representing the structure of the program.

For example:

int main()
{
    int x = 42;
    return x;
}

is represented internally as a program containing a main function, a variable declaration and a return statement.

Semantic Analyzer

The semantic analyzer verifies that the parsed program is valid according to the rules currently supported by K++.

The analyzer currently checks things such as:

existence of main()
supported return types
variable declarations
duplicate variables
use of undeclared variables
supported expressions
supported statements
K++ IR

After semantic analysis, the program is converted into the K++ Intermediate Representation (IR).

The IR provides an intermediate layer between the C++ frontend and the x86 backend.

For example:

FUNCTION main
DECLARE x
STORE x, 42
LOAD x
RETURN 0
END

The IR currently contains operations such as:

FunctionBegin
Declare
Store
Load
Print
Return
FunctionEnd

This intermediate representation allows the compiler backend to work independently from the original C++ syntax.

x86 Backend

The x86 backend translates K++ IR instructions into 32-bit x86 machine code.

The generated machine code is intended to execute directly on the x86 architecture supported by KolibriOS.

Example output:

K++ x86 BACKEND
===============

Machine code:

E8 5F 00 00 00 E9 93 00 00 00 ...

The backend is being developed incrementally as new language and IR features are added.

KolibriOS Executable Writer

K++ does not generate ELF executables.

Instead, K++ produces the executable format required by KolibriOS.

The executable writer creates a KolibriOS-compatible binary containing the generated x86 machine code and the required executable information.

The final result is a .bin executable that can be transferred to KolibriOS.

Example:

K++ Binary Writer
=================

Archivo generado: build/hello.raw

K++ KolibriOS Executable Writer
================================

Ejecutable generado: build/hello.bin
Código x86: 183 bytes
Current Features

K++ 0.9 currently contains the following compiler components:

C++ source lexer
Token system
C++ parser
Abstract Syntax Tree
Semantic analyzer
Variable declarations
Integer literals
Variable expressions
Return statements
Print statements
K++ Intermediate Representation
x86 backend
Binary writer
KolibriOS executable writer
CMake build system
Example

A simple K++ program:

int main()
{
    int x = 42;
    return x;
}

The compiler processes it through:

C++ Source
    │
    ▼
   Lexer
    │
    ▼
  Parser
    │
    ▼
    AST
    │
    ▼
Semantic Analyzer
    │
    ▼
   K++ IR
    │
    ▼
 x86 Backend
    │
    ▼
Binary Writer
    │
    ▼
KolibriOS .BIN

Compile it with:

./bin/kpp projects/variable.cpp
Build

K++ uses CMake to build the compiler.

From the project directory:

cd /home/saykol/C/KPP

cmake -S . -B build

cmake --build build -j$(nproc)

The compiler executable is generated as:

bin/kpp
Project Structure
KPP/
├── bin/
├── build/
├── include/
│   └── kpp/
├── projects/
├── src/
│   ├── frontend/
│   ├── backend/
│   └── runtime/
├── CMakeLists.txt
└── README.md

The project is intentionally divided into compiler components so that the frontend, IR, backend and runtime can evolve independently.

Development Status

K++ is an experimental compiler under active development.

The current version, K++ 0.9, represents an important stage in the development of the compiler: source code can pass through the complete compilation pipeline and produce a KolibriOS-compatible binary.

The compiler is not yet a complete implementation of the C++ language.

Future development will progressively expand:

expressions
arithmetic operations
variables and values
types
functions
control flow
code generation
runtime support
optimizations
additional C++ language features

The objective is to continue building the compiler step by step while keeping its internal architecture understandable and testable.

Goal

The long-term goal of K++ is to provide a native C++ development environment for KolibriOS, starting with a small compiler and progressively expanding its capabilities.

                    C++ SOURCE
                         │
                         ▼
                    K++ COMPILER
                         │
                         ▼
                    x86 MACHINE CODE
                         │
                         ▼
                   KOLIBRIOS .BIN
                         │
                         ▼
                      KOLIBRIOS

K++ is not intended to be a wrapper around an existing compiler.

It is being developed as an independent compiler project, with its own frontend, IR, backend and executable generation pipeline.


 evolution

 La llevaría hacia:

KPP/
│
├── include/
│   └── kpp/
│       ├── AST.hpp
│       ├── Token.hpp
│       ├── IR.hpp
│       ├── Lexer.hpp
│       ├── Parser.hpp
│       └── ...
│
├── src/
│
│   ├── frontend/
│   │   ├── Lexer.cpp
│   │   ├── Parser.cpp
│   │   └── SemanticAnalyzer.cpp
│   │
│   ├── ir/
│   │   └── IRGenerator.cpp
│   │
│   ├── backend/
│   │   │
│   │   ├── common/
│   │   │
│   │   ├── x86/
│   │   │   ├── X86Backend.cpp
│   │   │   └── X86Emitter.cpp
│   │   │
│   │   ├── arm64/
│   │   │   └── ARM64Backend.cpp
│   │   │
│   │   └── llvm/
│   │       ├── LLVMBackend.cpp
│   │       └── LLVMIRGenerator.cpp
│   │
│   ├── object/
│   │   ├── ELFWriter.cpp
│   │   ├── PEWriter.cpp
│   │   └── RawWriter.cpp
│   │
│   ├── targets/
│   │   ├── LinuxTarget.cpp
│   │   ├── WindowsTarget.cpp
│   │   ├── KolibriTarget.cpp
│   │   └── BareMetalTarget.cpp
│   │
│   └── runtime/
│       ├── KolibriRuntime.cpp
│       ├── LinuxRuntime.cpp
│       └── ...

Pero no movería todo de golpe.

Primera fase que haría ahora

Antes de tocar LLVM, haría una cosa fundamental:

Crear un concepto formal de Target

Por ejemplo:

kpp hello.cpp --target=kolibri-x86
kpp hello.cpp --target=linux-x86_64
kpp hello.cpp --target=windows-x86_64
kpp hello.cpp --target=linux-arm64
kpp hello.cpp --target=baremetal-arm64

Y K++ decide:

target
   │
   ├── architecture
   │      ├── x86
   │      ├── x86_64
   │      └── arm64
   │
   ├── object format
   │      ├── ELF
   │      ├── PE
   │      ├── Kolibri
   │      └── RAW
   │
   └── runtime
          ├── Linux
          ├── Windows
          ├── Kolibri
          └── Bare Metal

Esto convierte K++ de:

"un compilador para KolibriOS"

en:

"un compilador multiplataforma con KolibriOS como uno de sus targets."
