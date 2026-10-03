K++ 0.9


|      EAX | Función                                                | Utilidad para K++          |
| -------: | ------------------------------------------------------ | -------------------------- |
|     `-1` | `terminate thread/process`                             | **Terminar Stage**         |
|      `2` | `get the code of the pressed key`                      | Entrada teclado            |
|      `3` | `get system time`                                      | Tiempo                     |
|      `4` | `draw text string`                                     | **Mostrar mensajes/debug** |
|      `5` | `delay`                                                | Esperas                    |
|      `9` | `information on execution thread`                      | Información del proceso    |
|     `10` | `wait for event`                                       | Eventos                    |
|     `11` | `check for event, no wait`                             | Eventos sin bloquear       |
|     `14` | `get screen size`                                      | Pantalla                   |
|     `17` | `get the identifier of the pressed button`             | GUI                        |
|     `22` | `set system date/time`                                 | Fecha/hora                 |
|     `23` | `wait for event with timeout`                          | Eventos                    |
|     `29` | `get system date`                                      | Fecha                      |
|     `30` | `work with the current folder`                         | Directorio                 |
|     `37` | `work with mouse`                                      | Ratón                      |
|     `40` | `set the mask for expected events`                     | Eventos                    |
|     `47` | `draw a number in the window`                          | Debug/números              |
|     `60` | `Inter Process Communication (IPC)`                    | Comunicación               |
|     `61` | `get parameters for direct graphics access`            | Gráficos                   |
|     `64` | `resize application memory`                            | Memoria                    |
|     `66` | `work with keyboard`                                   | Teclado                    |
|     `67` | `change position/sizes of window`                      | Ventana                    |
| **`70`** | **`work with file system with long names support`**    | **⭐ ARCHIVOS K++**         |
|     `71` | `set window caption`                                   | Ventana                    |
|     `72` | `send message to a window`                             | GUI                        |
|     `73` | `blit bitmap`                                          | Gráficos                   |
| **`80`** | **`file system interface with parameter of encoding`** | **⭐ ARCHIVOS/ENCODING**    |


Function 70
    │
    ├── 0  READ_FILE
    ├── 1  READ_FOLDER
    ├── 2  CREATE_FILE
    ├── 3  WRITE_FILE
    ├── 4  SET_END
    ├── 5  GET_INFO
    ├── 6  SET_INFO
    ├── 7  START_APP
    ├── 8  DELETE
    ├── 9  CREATE_FOLDER
    ├── 10 RENAME/MOVE
    ├── 11 SYMLINK
    └── 12 READ_SYMLINK

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

FASE 1 — VARIABLES Y ASIGNACIÓN

┌──────────────────────────────┐
│ Variables                    │
│ declaración                  │
│ asignación                   │
│ LOAD / STORE                 │
│ RETURN                       │
└──────────────┬───────────────┘
               ↓
          🟢 COMPLETA
               ↓
FASE 2 — CONTROL DE FLUJO

┌──────────────────────────────┐
│ if                           │
│ else                         │
│ else if                      │
│ comparación   - x = suma();

             El error:

             K++ Parser error at 8:10: se esperaba '='      El mensaje:

Parser error at 8:10: se esperaba '='

corresponde justamente a:

int x;
     ^

El bloque de asignación independiente sí existe y está implementado correctamente:

x = suma();                    │
│ saltos                       │
│ scopes                       │
│ shadowing                    │
│ múltiples RETURN             │
└──────────────┬───────────────┘
               ↓
          🟢 COMPLETA
               ↓

               ↓
FASE 3 — CONTROL DE REPETICIÓN
┌──────────────────────────────┐
│ while                        │
│ mutación de variable         │
│ loops                        │
│                              │
│ condición                    │
│ cuerpo                       │
│ salto hacia atrás            │
│ salida del ciclo             │
│ variables dentro del loop    │
│ LOAD / STORE dentro del loop │
│ SUB dentro del loop          │
│ print_int dentro del loop    │
└──────────────┬───────────────┘
               ↓
          🟢 COMPLETA
               ↓

               | Izquierdo | Derecho | Estrategia                                             |
| --------- | ------- | ------------------------------------------------------ |
| EAX       | EBX     | `SUB EAX, EBX`                                         |
| EBX       | EAX     | intercambio mediante pila                              |
| MEMORY    | EAX     | recargar izquierdo conservando derecho                 |
| EAX       | MEMORY  | recargar derecho conservando izquierdo                 |
| EBX       | MEMORY  | conservar EBX, recargar derecho y después intercambiar |
| MEMORY    | EBX     | conservar EBX, recargar izquierdo                      |



FASE 4 — FUNCIONES
┌──────────────────────────────┐
│ funciones                    │
│                              │
│ declaración de funciones     │
│ parámetros                   │
│ llamadas                     │
│ argumentos                   │
│ retorno de valores           │
│                              │
│ ámbito de variables          │
│ variables locales            │
│ RETURN desde función         │
│                              │
│ salto / retorno              │
│ preservación de registros    │
└──────────────┬───────────────┘
               ↓
          🟡 EN DESARROLLO

────────────────────────────
FASE 4 — FUNCIONES


┌────────────────────────────────────┐
│ FASE 4.1 — Function calls          │
├────────────────────────────────────┤
│ Parser                             │ 🟢
│ CallExpression                     │ 🟢
│ Análisis semántico                 │ 🟢
│ Funciones declaradas después       │ 🟢
│ IR CALL                            │ 🟢
│ CALL x86 rel32                     │ 🟢
│ Retorno de función                 │ 🟢
│ RET                                │ 🟢
│ STORE del CALL                     │ 🟢
│ LOAD del CALL                      │ 🟢
│ PRINT_INT                          │ 🟢
│ Backend con múltiples funciones    │ 🟢
│ KolibriOS                          │ 🟢
└────────────────────────────────────┘



     ↓

FASE 4.2 — PARÁMETROS

4.2.1 Declaración
────────────────────────────────
Parameter AST                       🟢
FunctionDeclaration.parameters      🟢
Parser                              🟢
AST printer                         🟢
Compatibilidad con 4.1             🟢


4.2.2 Argumentos
────────────────────────────────
CallExpression.arguments            🟢
Parser de argumentos                🟢
Validación cantidad                 🟢
Validación tipos                    🟢
AST printer de argumentos           🟢


4.2.3 Paso de valores
────────────────────────────────
Relación parámetro ↔ argumento      🟢
Constantes                          🟢
Variables                           🟢
Expresiones                         🟢
Argumentos en registros             🟢
Argumentos en memoria/spill         🟢


4.2.4 CALL con argumentos
────────────────────────────────
CALL con 0 argumentos               🟢
CALL con constantes                 🟢
CALL con variables                  🟢
CALL con expresiones                🟢
CALL con 2 expresiones              🟢
Argumento 0 → EAX                   🟢
Argumento 1 → EBX                   🟢
Resultado → EAX                     🟢
KolibriOS                           🟢


4.2.5 RETURN
────────────────────────────────
RETURN desde función con parámetros 🟢
RETURN de expresión                 🟢
Resultado recibido por caller       🟢
KolibriOS                           🟢



────────────────────────────

4.3  Argumentos avanzados
     ⬜

     evaluación de argumentos
     múltiples expresiones
     composición de argumentos
     orden de evaluación
     argumentos anidados
     argumentos provenientes de CALL
     presión de registros
     argumentos con temporales en memoria

     ↓

4.4  Llamadas / composición
     ⬜

     función → función
     CALL dentro de expresión
     CALL como argumento de otra función
     composición:

         suma(2, 3)
         suma(suma(1,2), 4)

     múltiples CALL consecutivos
     CALL anidados
     resultados temporales entre CALL
     composición profunda

     ↓

4.5  Valores de retorno
     ⬜

     RETURN constante
     RETURN variable
     RETURN expresión
     RETURN CALL
     retorno entre funciones
     retorno de funciones anidadas
     recepción del resultado
     preservación del valor retornado

     ↓

4.6  Variables locales / scopes
     ⬜

     variables locales por función
     scope de parámetros
     scope de variables
     shadowing
     misma variable en funciones diferentes
     lifetime lógico de variables
     contexto de variables por función
     LOAD / STORE por scope
     interacción entre parámetros y locales

     ↓

4.7  CALL / RET + preservación de registros
     ⬜

     ABI interno de K++

     EAX
     EBX
     ECX
     EDX

     registros de argumentos
     registro de retorno
     registros caller-saved
     registros callee-saved
     temporales vivos durante CALL
     spills antes de CALL
     restauración después de CALL
     preservación entre CALL y CALL
     CALL dentro de expresiones
     RET seguro

     ↓

     🟢 ABI de funciones estable2
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


FASE 5 — TIPOS
FASE 6 — ESTRUCTURAS DE DATOS
FASE 7 — ARRAYS
FASE 8 — STRINGS
FASE 9 — POINTERS / MEMORY
FASE 10 — RUNTIME K++
FASE 11 — ABI K++ / KOLIBRIOS
FASE 12 — I/O KOLIBRIOS
FASE 13 — GUI KOLIBRIOS
FASE 14 — SELF-HOSTING


| Componente              | Estado actual                                                                                                                              |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| **Lexer**               | 🟢 Funcional para el conjunto actual de tokens, variables, operadores, `if`, `else`, `return`, `print`, etc.                               |
| **Parser**              | 🟢 Funcional para declaraciones, expresiones, variables, operadores, `if`, `else`, `else if`, `return` y bloques                           |
| **AST**                 | 🟢 Representa variables, expresiones, `if`, `else`, `else if`, `return`, bloques y llamadas/impresiones necesarias                         |
| **SemanticAnalyzer**    | 🟢 Valida variables, expresiones, condiciones, `return` y estructura actual de `if/else`                                                   |
| **IR**                  | 🟢 Infraestructura funcional con variables, `LOAD`, `STORE`, aritmética, comparaciones, `LABEL`, `JUMP`, `JUMP_IF_ZERO`, `RETURN`, `PRINT` |
| **IRGenerator**         | 🟢 Conecta variables, `LOAD`, `STORE`, `if`, `else`, `else if`, comparaciones, `return` y salida al IR                                     |
| **Allocator**           | 🟢 Funcional para el conjunto actual, incluyendo EAX/EBX y spills/reloads                                                                  |
| **X86Emitter**          | 🟢 Genera instrucciones aritméticas, `CMP`, `SETcc`, `MOVZX`, `JMP`, saltos condicionales y patching                                       |
| **X86Backend**          | 🟢 Control de flujo, comparaciones, `JUMP`, `JUMP_IF_ZERO`, `if`, `else` y `else if` funcionales                                           |
| **KolibriRuntime**      | 🟢 Funcional; ventana, `PRINT` y `PRINT_INTEGER` probados en KolibriOS                                                                     |
| **BinaryWriter**        | 🟢 Probado generando ejecutables `.BIN`                                                                                                    |
| **Ejecución KolibriOS** | 🟢 **Probada extremo a extremo**                                                                                                           |
| **Self-hosting**        | 🔴 Todavía falta suficiente lenguaje para que K++ pueda compilarse a sí mismo                                                              |
                                                        |



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
