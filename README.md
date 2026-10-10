# K++ --- Compilador experimental para KolibriOS

> **Versión documentada:** K++ 0.9\
> **Estado:** desarrollo experimental activo\
> **Objetivo principal:** construir un compilador independiente que
> traduzca un subconjunto creciente de C++ a ejecutables nativos de
> KolibriOS.

K++ es un proyecto de compilador desarrollado desde cero. Cuenta con
frontend propio, representación intermedia (IR), backend x86 y generador
de ejecutables compatibles con KolibriOS. No pretende ser simplemente
una envoltura alrededor de un compilador existente.

El proyecto avanza por etapas: primero consolida un subconjunto pequeño
y comprobable del lenguaje; después amplía sus tipos, funciones, memoria
y runtime; finalmente busca compilar su propio código fuente
(*self-hosting*).

> **Nota sobre el estado:** este documento distingue entre arquitectura
> existente, funcionalidades declaradas, pruebas confirmadas y trabajo
> pendiente. Que una característica esté implementada en alguna capa no
> implica que todas sus combinaciones estén verificadas de extremo a
> extremo.

------------------------------------------------------------------------

## Índice

-   [Objetivos](#objetivos)
-   [Estado general](#estado-general)
-   [Arquitectura del compilador](#arquitectura-del-compilador)
-   [Estructura del repositorio](#estructura-del-repositorio)
-   [Requisitos y construcción](#requisitos-y-construcción)
-   [Uso básico](#uso-básico)
-   [Capacidades del lenguaje](#capacidades-del-lenguaje)
-   [Funciones, parámetros y ABI](#funciones-parámetros-y-abi)
-   [Estado de las pruebas](#estado-de-las-pruebas)
-   [Limitaciones conocidas](#limitaciones-conocidas)
-   [Llamadas al sistema de
    KolibriOS](#llamadas-al-sistema-de-kolibrios)
-   [Hoja de ruta](#hoja-de-ruta)
-   [Plan de bootstrap y
    self-hosting](#plan-de-bootstrap-y-self-hosting)
-   [Evolución multiplataforma](#evolución-multiplataforma)
-   [Contribución y criterios de
    calidad](#contribución-y-criterios-de-calidad)

------------------------------------------------------------------------

## Objetivos

Los objetivos de K++ son:

-   Desarrollar un compilador con frontend, IR y backend propios.
-   Traducir programas del subconjunto admitido de C++ a código máquina
    x86 de 32 bits.
-   Generar ejecutables `.BIN` compatibles con KolibriOS, en lugar de
    ejecutables ELF.
-   Mantener las etapas del compilador separadas para poder
    desarrollarlas y probarlas independientemente.
-   Ampliar gradualmente el lenguaje con control de flujo, funciones,
    tipos, estructuras de datos y runtime.
-   Construir una suite reproducible de pruebas.
-   Escribir progresivamente el compilador en K++ y alcanzar la
    autocompilación.
-   Mantener abierta la arquitectura para futuros targets, sin
    comprometer prematuramente el backend actual.

## Estado general

### Componentes principales

  -----------------------------------------------------------------------
  Componente                          Estado documentado
  ----------------------------------- -----------------------------------
  Lexer y sistema de tokens           Funcionales para el conjunto de
                                      sintaxis implementado; la cobertura
                                      debe comprobarse con la suite

  Parser                              Declaraciones, expresiones y
                                      construcciones de control
                                      documentadas para el subconjunto
                                      actual

  AST                                 Representación de declaraciones,
                                      expresiones, bloques y
                                      construcciones implementadas

  Analizador semántico                Validaciones para las
                                      características soportadas; quedan
                                      casos por verificar

  IR de K++                           Infraestructura para valores,
                                      variables, operaciones, saltos,
                                      funciones y retorno

  Generador de IR                     Traduce las construcciones
                                      implementadas al IR

  Allocator                           Gestión de registros EAX/EBX y
                                      temporales en memoria
                                      (*spills/reloads*) para los casos
                                      cubiertos

  Backend x86                         Emisión incremental de
                                      instrucciones y saltos

  BinaryWriter                        Generación de artefactos binarios

  KolibriOS Executable Writer         Generación del formato ejecutable
                                      destinado a KolibriOS

  Runtime de KolibriOS                Salida y operaciones básicas
                                      documentadas; ampliar y verificar
                                      la cobertura

  Ejecución en KolibriOS              Hay pruebas de integración extremo
                                      a extremo documentadas

  Self-hosting                        Pendiente; no debe considerarse
                                      alcanzado
  -----------------------------------------------------------------------

### Alcance actual

K++ **no es todavía una implementación completa de C++**. Las
capacidades reales dependen del estado de cada fase del frontend, IR,
backend y runtime. El soporte declarado para una construcción debe
confirmarse mediante pruebas reproducibles antes de considerarlo
estable.

La documentación de desarrollo registra que variables, asignaciones,
`LOAD`/`STORE`, `return`, comparaciones, `if`/`else` y `while` han
avanzado hasta completarse en sus respectivas fases. Las llamadas a
funciones y los parámetros tienen una base funcional, pero las
expresiones anidadas, la presión de registros y la preservación completa
de temporales requieren validación específica.

------------------------------------------------------------------------

## Arquitectura del compilador

K++ separa el proceso de compilación en las siguientes etapas:


Código fuente C++ / K++ admitido
              |
              v
            Lexer
              |
              v
            Tokens
              |
              v
            Parser
              |
              v
             AST
              |
              v
      Semantic Analyzer
              |
              v
          K++ IR
              |
              v
         X86 Backend
              |
              v
         Binary Writer
              |
              v
   KolibriOS Executable Writer
              |
              v
        Ejecutable .BIN
              |
              v
           KolibriOS


### 1. Lexer

Lee el código fuente y lo convierte en tokens. Entre las categorías
relevantes están:

-   Identificadores.
-   Números y literales.
-   Palabras clave.
-   Operadores.
-   Paréntesis y llaves.
-   Punto y coma.
-   Cadenas, según el soporte implementado.

La cobertura de cada token debe verificarse en las pruebas del lexer.

### 2. Parser y AST

El parser consume los tokens y construye un árbol de sintaxis abstracta
(*Abstract Syntax Tree*, AST). El AST representa la estructura del
programa y sirve de entrada al análisis semántico y a la generación de
IR.

Ejemplo de programa:


cpp

int main()
{
    int x = 42;
    return x;
}


Conceptualmente, el árbol contiene una función `main`, una declaración
de variable y una instrucción de retorno.
La entrada y salida pertenecen al entorno de K++ y deben integrarse con su IR, backend y runtime.

No se requiere #include <iostream> ni se presupone la disponibilidad de la biblioteca estándar de C++.
No posee las librerias de C++ ni su entorno, se ejecuta bajo entorno propio.
print es la operación propia para mostrar datos.
input es la operación propia para recibir datos.



### 3. Analizador semántico

Comprueba que el programa respete las reglas que K++ implementa
actualmente. Entre las validaciones registradas están:

-   Existencia y forma de `main()`.
-   Tipos de retorno admitidos.
-   Declaraciones y usos de variables.
-   Detección de declaraciones duplicadas en los ámbitos cubiertos.
-   Uso de variables no declaradas.
-   Validez de expresiones y sentencias admitidas.
-   Validación de argumentos de llamadas en los casos implementados.

El análisis semántico debe mantenerse alineado con el parser, el AST y
el backend: aceptar una construcción en el frontend no basta si luego no
puede generar código correcto.

### 4. Representación intermedia (IR)

El IR separa la sintaxis del lenguaje fuente de los detalles de la
arquitectura de destino.

Las operaciones documentadas incluyen, según el estado de
implementación:

-   Constantes y operaciones aritméticas.
-   Declaración de variables.
-   `LOAD` y `STORE`.
-   Comparaciones y operaciones lógicas.
-   Etiquetas y saltos.
-   Saltos condicionales.
-   Llamadas y retorno.
-   Impresión.

Ejemplo ilustrativo de IR:


FUNCTION main
DECLARE x
STORE x, 42
LOAD x
RETURN 0
END


Este ejemplo explica el concepto; no constituye una garantía de que el
volcado exacto del IR actual use literalmente ese formato.

### 5. Backend x86

Traduce las instrucciones del IR al código máquina x86 de 32 bits
destinado al target actual de KolibriOS. La generación se amplía a
medida que se incorporan operaciones y construcciones nuevas.

Entre las instrucciones y mecanismos documentados están las operaciones
aritméticas, `CMP`, `SETcc`, `MOVZX`, `JMP`, saltos condicionales,
llamadas relativas y resolución (*patching*) de destinos de salto.

### 6. Generación del ejecutable

K++ separa la escritura del código binario de la escritura del
ejecutable específico de KolibriOS. El resultado de destino es un
archivo `.BIN` compatible con ese sistema operativo; K++ no genera ELF
para su target actual.

Los nombres y rutas exactos de los artefactos dependen de la
configuración de compilación utilizada.

------------------------------------------------------------------------

## Estructura del repositorio

La siguiente estructura reúne los archivos descritos en la documentación
disponible. La distribución real puede evolucionar con el desarrollo:

```
$ tree
.
├── bin
│   └── kpp
├── bootstrap
│   ├── scripts
│   ├── stage0
│   │   └── README.md
│   ├── stage1
│   │   └── README.md
│   └── stage2
│       └── README.md
├──

├── compiler
│   └── kpp
│       ├── ast
│       ├── backend
│       ├── ir
│
│       ├── lexer
│       │   └── Lexer.kpp
│       ├── main.kpp
│       ├── parser
│       ├── semantic
│
├── include
│   └── kpp
│       ├── AST.hpp
│       ├── BinaryWriter.hpp
│       ├── IRGenerator.hpp
│       ├── IR.hpp
│       ├── KolibriExecutableWriter.hpp
│       ├── KolibriRuntime.hpp
│       ├── Lexer.hpp
│       ├── Parser.hpp
│       ├── SemanticAnalyzer.hpp
│       ├── Token.hpp
│       ├── X86Backend.hpp
│       └── X86Emitter.hpp
├── kpp_bootstrap_studio.py
├── kpp.cpp
├── Output
│
├── projects
│
├── README.md
├── src
│   ├── backend
│   │   ├── BinaryWriter.cpp
│   │   ├── IRGenerator.cpp
│   │   ├── KolibriExecutableWriter.cpp
│   │   ├── X86Backend.cpp
│   │   └── X86Emitter.cpp
│   ├── frontend
│   │   ├── Lexer.cpp
│   │   ├── Parser.cpp
│   │   └── SemanticAnalyzer.cpp
│   ├── main.cpp
│   └── runtime
│       └── KolibriRuntime.cpp
└── temporal

33 directories, 248 files

$

```

### Responsabilidades

  -----------------------------------------------------------------------
  Ruta                                Responsabilidad
  ----------------------------------- -----------------------------------
  `include/kpp/`                      Interfaces y declaraciones de los
                                      componentes

  `src/frontend/`                     Lexer, parser y análisis semántico

  `src/backend/`                      Generación de IR, emisión x86 y
                                      escritura de binarios

  `src/runtime/`                      Funciones de runtime relacionadas
                                      con KolibriOS

  `projects/`                         Programas de ejemplo y pruebas
                                      manuales

  `build/`                            Archivos generados por CMake y la
                                      compilación

  `bin/`                              Ubicación documentada para el
                                      ejecutable del compilador

  `temporal/`                         Material temporal de desarrollo
  -----------------------------------------------------------------------

Los directorios `build/` y los binarios generados son artefactos de
construcción, no sustitutos del código fuente.

------------------------------------------------------------------------

## Requisitos y construcción

El proyecto utiliza CMake y un compilador C++ compatible con el código
del repositorio.

Desde el directorio raíz:

``` bash
cd /home/saykol/C/KPP

cmake -S . -B build
cmake --build build -j"$(nproc)"
```

La ruta del proyecto anterior corresponde al entorno de desarrollo
documentado; debe ajustarse si el repositorio está en otra ubicación.

El ejecutable del compilador se documenta como:

```
bin/kpp
```

Si la configuración actual de CMake genera el ejecutable en otra ruta,
debe utilizarse la ruta que muestre la compilación.

### Compilación de un ejemplo

``` bash
./bin/kpp projects/variable.cpp
```

La salida esperada es la generación de artefactos de compilación y,
cuando el proceso termina correctamente, un ejecutable compatible con el
target configurado. La ruta exacta debe comprobarse en la salida real
del compilador.

Ejemplo de nombres documentados:

``` text
build/hello.raw
build/hello.bin
```

`*.raw` representa el artefacto de código/binario intermedio descrito en
el proyecto, mientras que `*.bin` representa el ejecutable de KolibriOS.
No se debe asumir que todos los programas producen exactamente esos
nombres sin revisar la configuración actual.

------------------------------------------------------------------------

## Capacidades del lenguaje

La tabla resume las funciones registradas en la documentación de
desarrollo. Los estados deben leerse junto con la tabla de pruebas:
**declarado** no significa que todos los casos de uso estén verificados.

  -----------------------------------------------------------------------
  Característica                      Estado documentado
  ----------------------------------- -----------------------------------
  `int`                               Declarado como tipo base

  Variables e inicializadores         Implementación documentada;
                                      consolidar con pruebas

  Expresiones aritméticas             Implementación documentada

  `return`                            Implementado en el subconjunto
                                      actual

  Literales de impresión              Declarados en Stage 0

  Funciones                           Base de llamadas y retornos
                                      implementada

  Parámetros y argumentos             Implementación inicial documentada

  `if` / `else` / `else if`           Fase de control de flujo marcada
                                      como completa; verificar
                                      regresiones

  `while`                             Fase de repetición marcada como
                                      completa; verificar regresiones

  Comparaciones                       Implementación documentada

  Operadores lógicos                  Cobertura por confirmar en la suite
                                      completa

  `struct`                            Declaración reconocida en pruebas
                                      anteriores; acceso a campos
                                      incompleto

  `enum`                              Hay pruebas específicas; consolidar
                                      ejecución e integración

  `string`                            Aparece en los requisitos de Stage
                                      0; soporte completo por confirmar

  `void` y tipos adicionales          No asumir soporte: la documentación
                                      indica que el lenguaje operativo
                                      todavía está limitado
                                      principalmente a `int`

  Arrays y punteros                   Pendientes en la hoja de ruta

  Recursión                           Pendiente de confirmación mediante
                                      pruebas

  Self-hosting                        No alcanzado
  -----------------------------------------------------------------------

### Ejemplo básico

kpp
int main()
{
    int x = 42;
    return x;
}


Este ejemplo representa el subconjunto mínimo de función, variable
inicializada y retorno descrito por el proyecto. Debe compilarse con la
versión concreta de K++ para comprobar el comportamiento actual.

### Tipos contemplados a futuro

Los siguientes tipos aparecen en la planificación, pero **no deben
interpretarse como soporte completo disponible**:

  Tipo                           Uso previsto
  ------------------------------ --------------------------------
  `void`                         Funciones sin valor de retorno
  `bool`                         Valores verdadero/falso
  `double`                       Decimales de doble precisión
  `float`                        Decimales
  `char`                         Caracteres
  `long`, `short`, `long long`   Variantes de enteros
  `unsigned int`                 Enteros sin signo

La incorporación de un tipo exige definir su representación, reglas
semánticas, operaciones admitidas, convención de llamada y generación de
código.

------------------------------------------------------------------------

## Funciones, parámetros y ABI

Las llamadas a funciones son una parte central del desarrollo actual.

### Base documentada de funciones

Los elementos marcados como implementados en el registro de la fase 4.1
incluyen:

-   `CallExpression` y análisis semántico de llamadas.
-   Funciones declaradas después de sus llamadas, según las pruebas
    registradas.
-   Instrucción `CALL` en el IR.
-   Emisión de `CALL rel32` en x86.
-   Retorno de función y `RET`.
-   Lectura y almacenamiento de resultados de llamadas.
-   Backend con múltiples funciones.
-   Ejecución del resultado en KolibriOS.

### Parámetros y argumentos

La fase 4.2 registra soporte inicial para:

-   Declaración y representación AST de parámetros.
-   Argumentos en `CallExpression`.
-   Validación de cantidad y tipos en los casos cubiertos.
-   Asociación parámetro--argumento.
-   Constantes, variables y expresiones como argumentos.
-   Argumentos en registros y temporales en memoria (*spill*).
-   Recepción del resultado de una llamada mediante `EAX`.

La documentación identifica `EAX` como primer registro de argumento,
`EBX` como segundo y `EAX` como registro de retorno en la convención
interna descrita. Esta convención es específica del diseño actual de K++
y no debe confundirse con una ABI estándar de plataformas como Linux
x86-64.

### ABI interno y preservación de registros

Registros mencionados en la planificación del ABI:

-   `EAX`   `Acumulador; resultados y valores de retorno.`
-   `EBX`   `Registro base; puede conservar valores.`
-   `ECX`   `Contador; bucles y desplazamientos.`
-   `EDX`   `Datos; operaciones aritméticas y E/S.`
-   `ESI`   `Índice de origen; operaciones con datos.`
-   `EDI`   `Índice de destino; operaciones con datos.`
     EBP 	   Base del marco de pila.
     ESP 	   Puntero de pila.

La fase avanzada debe definir de forma consistente:

-   Registros usados para argumentos y valores de retorno.
-   Registros que puede modificar el caller y los que debe preservar el
    callee.
-   Temporales que siguen vivos a través de un `CALL`.
-   *Spills* antes de una llamada y restauración después de ella.
-   Llamadas anidadas y consecutivas.
-   Seguridad del retorno y conservación del valor retornado.

El documento de desarrollo marca el ABI de funciones como estable para
los casos ya implementados, pero mantiene pendiente la cobertura
avanzada de composición de expresiones y presión de registros.

### Casos avanzados pendientes

``` cpp
suma(2, 3);
suma(suma(1, 2), 4);
```

Estos ejemplos ilustran llamadas compuestas que deben formar parte de
las pruebas futuras. La suite debe verificar llamadas dentro de
expresiones, llamadas como argumentos, resultados temporales entre
llamadas y composición profunda.

------------------------------------------------------------------------

## Estado de las pruebas

Las pruebas L01--L15 constituyen una matriz de cobertura del lenguaje.
El estado que se muestra aquí conserva las diferencias registradas entre
pruebas confirmadas y pendientes.

  -----------------------------------------------------------------------
  ID                      Área                     Estado documentado
  ----------------------- ----------------------- -----------------------
  L01       PASS          Tipos `int` y `string`    Declarada en Stage 0;


  L02       PASS          Variables e               Declarada en Stage 0;
                          inicializadores

  L03       FAIL          Operaciones aritméticas   Declarada en Stage 0;
                                                      + y - funcionan;
                                                      faltan *, / y %


  L04       PASS          Funciones y retornos       Declarada en Stage 0;


  L05       PASS          Parámetros, argumentos     Declarada en Stage 0;
                          y llamadas

  L06       PASS          Impresión de literales     Declarada en Stage 0;


  L07       PASS              `struct`               Declarada en Stage 0;

  L08       PASS               `enum`                Declarada en Stage 0;

  L09       PASS             Operadores              La documentación
                              lógicos                registra una
                                                     integración ejecutada
                                                     en KolibriOS

  L10       PASS             `if` / `else`           Declarada en Stage 0;

  L11       PASS             `while`                 Declarada en Stage 0;

  L12       FAIL
            : shadowing                              Variables locales; se registra
                                                     alcance de bloques
                                                     un fallo de *shadowing*

  L13       Bloqueada:      `Campos de struct`       . no reconocido por el lexer



  L14       PASS           Errores sintácticos y   Pendiente de confirmar
                           semánticos              ejecución

  L15       Pendiente     Recursión               Pendiente de confirmar
                                                  ejecución

  L16        FAIL         Llamadas anidadas       Análisis semántico  PASS
                                                  Generación de IR    PASS
                                                  Backend x86         FAIL

  -----------------------------------------------------------------------

### Detalle de L13: campos de `struct`

  Caso                                Estado registrado
  ----------------------------------- -----------------------------------------
  Declaración de `struct`             PASS en pruebas anteriores del frontend
  Rechazo de campos duplicados        PASS
  Rechazo de estructuras duplicadas   PASS
  Token `.` en el lexer               FAIL confirmado
  Acceso `p.edad`                     Pendiente
  Asignación y lectura de campos      Pendiente
  Independencia entre variables       Pendiente
  Rechazo de campos inexistentes      Pendiente
  Rechazo de tipos incompatibles      Pendiente

El fallo del token `.` impide considerar completo el acceso a campos. La
declaración de estructuras y la validación de nombres duplicados no
demuestran, por sí solas, que los miembros funcionen de extremo a
extremo.

### Requisitos mínimos de Stage 0

Antes de utilizar Stage 0 como base del bootstrap, conviene consolidar
una suite que compruebe:

1.  Tipos admitidos.
2.  Variables e inicializadores.
3.  Aritmética y precedencia.
4.  Funciones y retornos.
5.  Parámetros, argumentos y llamadas.
6.  Impresión de literales.
7.  Declaración de estructuras.
8.  Declaración y uso de enumeraciones.
9.  Operadores relacionales y lógicos.
10. Condicionales.
11. Bucles.
12. Ámbitos y variables locales.
13. Campos de estructuras.
14. Errores sintácticos y semánticos.
15. Recursión.

Cada prueba debería registrar el código fuente, el comando usado, el
resultado esperado, el resultado real y el entorno de ejecución. Los
estados PASS/FAIL deben proceder de ejecuciones reproducibles y no solo
de la existencia de un archivo de prueba o de un binario antiguo.

------------------------------------------------------------------------

## Limitaciones conocidas

Según el registro disponible, las áreas que requieren atención especial
son:

-   **Shadowing y scopes:** L12 registra un fallo que debe resolverse y
    cubrirse con pruebas.
-   **Acceso a campos de `struct`:** el token `.` tiene un fallo
    confirmado; acceso, asignación y validaciones siguen pendientes.
-   **Tipos:** no debe afirmarse que `void`, `bool`, `float`, `double` u
    otros tipos están plenamente implementados.
-   **Expresiones con llamadas:** faltan pruebas avanzadas de llamadas
    anidadas y composición.
-   **Preservación de registros:** se debe probar la vida de temporales
    antes, durante y después de llamadas.
-   **Runtime e I/O:** la cobertura documentada no equivale a un runtime
    completo.
-   **Self-hosting:** el compilador aún necesita más capacidades del
    lenguaje y una cadena de bootstrap reproducible.
-   **Targets adicionales:** ARM64, Linux, Windows y bare metal son
    líneas futuras, no targets que este README declare ya funcionales.

------------------------------------------------------------------------

## Llamadas al sistema de KolibriOS

La documentación del proyecto destaca las funciones del sistema
operativo relevantes para un futuro runtime K++.

  ------------------------------------------------------------------------
                           EAX Función documentada   Uso potencial
  ---------------------------- --------------------- ---------------------
                          `-1` Terminar hilo/proceso Finalizar un programa

                           `2` Obtener código de     Entrada de teclado
                               tecla pulsada

                           `3` Obtener hora del      Tiempo
                               sistema

                           `4` Dibujar una cadena de Mensajes y depuración
                               texto

                           `5` Retardo               Esperas

                           `9` Información del hilo  Información del
                               de ejecución          proceso

                          `10` Esperar un evento     Gestión de eventos

                          `11` Comprobar evento sin  Eventos no
                               esperar               bloqueantes

                          `14` Obtener tamaño de     Interfaz gráfica
                               pantalla

                          `17` Obtener identificador GUI
                               del botón pulsado

                          `22` Establecer fecha/hora Fecha y hora
                               del sistema

                          `23` Esperar evento con    Eventos temporizados
                               timeout

                          `29` Obtener fecha del     Fecha
                               sistema

                          `30` Operar con el         Directorios
                               directorio actual

                          `37` Trabajar con el ratón Entrada de ratón

                          `40` Establecer máscara de Eventos
                               eventos

                          `47` Dibujar un número en  Depuración
                               la ventana

                          `60` Comunicación entre    Comunicación
                               procesos (IPC)

                          `61` Obtener parámetros    Gráficos
                               para acceso gráfico
                               directo

                          `64` Redimensionar memoria Memoria
                               de la aplicación

                          `66` Trabajar con el       Entrada
                               teclado

                          `67` Cambiar               Ventanas
                               posición/tamaño de
                               ventana

                          `70` Interfaz de sistema   Operaciones con
                               de archivos con       archivos
                               nombres largos

                          `71` Establecer título de  Ventanas
                               ventana

                          `72` Enviar mensaje a una  GUI
                               ventana

                          `73` Blit de bitmap        Gráficos

                          `80` Interfaz de archivos  Archivos y
                               con parámetro de      codificación
                               codificación
  ------------------------------------------------------------------------

### Subfunciones documentadas de la función 70

    Código Operación
  -------- -----------------
       `0` `READ_FILE`
       `1` `READ_FOLDER`
       `2` `CREATE_FILE`
       `3` `WRITE_FILE`
       `4` `SET_END`
       `5` `GET_INFO`
       `6` `SET_INFO`
       `7` `START_APP`
       `8` `DELETE`
       `9` `CREATE_FOLDER`
      `10` `RENAME/MOVE`
      `11` `SYMLINK`
      `12` `READ_SYMLINK`

> Esta tabla organiza la referencia recopilada para el proyecto; antes
> de implementar llamadas de sistema, deben verificarse los parámetros,
> registros, convenciones y versiones aplicables en la documentación de
> KolibriOS. No debe usarse como sustituto de esa referencia técnica.

------------------------------------------------------------------------

## Hoja de ruta

La planificación agrupa el desarrollo en fases. Las fases marcadas como
completas corresponden al registro del proyecto; los hitos futuros
dependen de las pruebas de integración.

### Fase 1 --- Variables y asignación

-   Declaración y asignación de variables.
-   `LOAD` / `STORE`.
-   `RETURN`.

**Estado registrado:** completa.

### Fase 2 --- Control de flujo

-   `if`, `else` y `else if`.
-   Comparaciones y saltos.
-   Scopes y *shadowing*.
-   Múltiples retornos.

**Estado registrado:** completa en la planificación; mantener pruebas de
regresión, especialmente para el alcance de variables.

### Fase 3 --- Repetición

-   `while`.
-   Mutación de variables.
-   Condición, cuerpo, salto hacia atrás y salida del ciclo.
-   Variables y `LOAD` / `STORE` dentro de bucles.
-   Operaciones aritméticas e impresión dentro del bucle.

**Estado registrado:** completa en la planificación; verificar los casos
integrales en la suite.

### Fase 4 --- Funciones

#### Fase 4.1: llamadas

Base de llamadas, retorno, `CALL rel32`, `RET`, múltiples funciones y
almacenamiento/lectura del resultado.

**Estado registrado:** componentes principales marcados como
implementados.

#### Fase 4.2: parámetros

Declaración de parámetros, argumentos, validación, paso de valores y
retorno desde funciones con parámetros.

**Estado registrado:** los casos básicos enumerados están marcados como
implementados.

#### Fase 4.3: argumentos avanzados

-   Evaluación de múltiples expresiones.
-   Composición de argumentos.
-   Orden de evaluación.
-   Argumentos anidados.
-   Argumentos provenientes de otras llamadas.
-   Presión de registros.
-   Temporales en memoria.

**Estado:** pendiente.

#### Fase 4.4: composición de llamadas

-   Función que llama a otra función.
-   `CALL` dentro de expresiones.
-   `CALL` como argumento.
-   Múltiples llamadas consecutivas.
-   Llamadas anidadas y composición profunda.
-   Gestión de resultados temporales entre llamadas.

**Estado:** pendiente.

#### Fase 4.5: valores de retorno

-   Retorno de constantes, variables y expresiones.
-   Retorno de una llamada.
-   Retornos entre funciones y funciones anidadas.
-   Recepción y preservación del valor devuelto.

**Estado:** ampliar y consolidar pruebas.

#### Fase 4.6: variables locales y scopes

-   Variables locales por función.
-   Scope de parámetros.
-   Shadowing.
-   Mismo nombre de variable en funciones diferentes.
-   Vida lógica de las variables.
-   `LOAD` / `STORE` por ámbito.
-   Interacción entre parámetros y variables locales.

**Estado:** pendiente de consolidación; existe un fallo de shadowing
registrado.

#### Fase 4.7: ABI y preservación de registros

-   Formalizar la ABI interna de K++.
-   Definir registros para argumentos y retorno.
-   Definir registros caller-saved y callee-saved.
-   Proteger temporales vivos durante llamadas.
-   Implementar spills y restauración.
-   Probar llamadas consecutivas y anidadas.
-   Garantizar un `RET` seguro.

**Estado:** base de ABI descrita como estable para los casos
implementados; cobertura avanzada pendiente.

### Fases posteriores

  -----------------------------------------------------------------------
  Fase                    Área                    Objetivo
  ----------------------- ----------------------- -----------------------
  5                       Tipos                   Ampliar los tipos
                                                  admitidos y sus reglas

  6                       Estructuras de datos    Definir estructuras y
                                                  su representación

  7                       Arrays                  Soportar colecciones
                                                  indexadas

  8                       Strings                 Consolidar cadenas y
                                                  sus operaciones

  9                       Punteros y memoria      Acceso y gestión de
                                                  memoria

  10                      Runtime K++             Primitivas e
                                                  infraestructura de
                                                  ejecución

  11                      ABI K++ / KolibriOS     Formalizar
                                                  interoperabilidad y
                                                  convenciones

  12                      I/O de KolibriOS        Entrada/salida del
                                                  sistema

  13                      GUI de KolibriOS        Ventanas, eventos y
                                                  gráficos

  14                      Self-hosting            Compilar el compilador
                                                  con K++
  -----------------------------------------------------------------------

Estas fases son una hoja de ruta, no una declaración de que las
características ya estén disponibles.

------------------------------------------------------------------------

## Plan de bootstrap y self-hosting

El objetivo de *self-hosting* es que K++ pueda compilar el código fuente
de su propio compilador sin depender del compilador C++ original para
ejecutar el proceso de compilación de programas.

### Etapas de bootstrap

```
Stage 0
Compilador actual, construido con la herramienta anfitriona
       |
       | compila el código fuente del compilador escrito en K++
       v
Stage 1
Compilador generado por Stage 0
       |
       | compila de nuevo las mismas fuentes
       v
Stage 2
Segunda generación que debe superar las mismas pruebas
```

### Requisitos mínimos de Stage 0

La documentación define un subconjunto inicial que incluye tipos
básicos, variables, expresiones, funciones, parámetros, impresión,
estructuras y enumeraciones. Las características que figuran como
"verificar" deben confirmarse antes de depender de ellas para compilar
el propio compilador.

### Criterios verificables

  -----------------------------------------------------------------------
  ID                      Requisito               Criterio de aceptación
  ----------------------- ----------------------- -----------------------
  B-01                    Código del compilador   Los componentes
                          escrito en K++          principales están
                                                  escritos en K++, no
                                                  delegados al compilador
                                                  C++ original

  B-02                    Compilación con Stage 0 Stage 0 genera el
                                                  ejecutable Stage 1 a
                                                  partir de fuentes K++

  B-03                    Independencia del       Stage 1 compila
                          compilador original     programas sin necesitar
                                                  ejecutar Stage 0 ni
                                                  invocar el compilador
                                                  C++ anfitrión

  B-04                    Pruebas funcionales     Stage 1 compila y
                                                  ejecuta correctamente
                                                  la suite acordada

  B-05                    Segunda generación      Stage 1 compila las
                                                  mismas fuentes y
                                                  produce Stage 2

  B-06                    Equivalencia de         Stage 1 y Stage 2
                          comportamiento          superan la misma suite
                                                  funcional

  B-07                    Reproducibilidad        Las generaciones pueden
                                                  repetirse siguiendo un
                                                  procedimiento
                                                  documentado y comparar
                                                  sus resultados
  -----------------------------------------------------------------------

### Hitos de aceptación

1.  **Stage 0 consolidado:** suite automatizada, inventario de
    características y defectos conocidos; los casos básicos deben ser
    reproducibles.
2.  **Subconjunto de Stage 1 cerrado:** gramática, tipos, modelo de
    ejecución y limitaciones documentados.
3.  **Compilador mínimo escrito en K++:** lexer, parser, análisis
    semántico y generación de código suficientes para programas
    pequeños.
4.  **Bootstrap:** Stage 0 compila Stage 1 y el ejecutable generado
    supera las pruebas.
5.  **Autocompilación:** Stage 1 compila sus propias fuentes y genera
    Stage 2, que vuelve a superar las pruebas.

### Estrategia de implementación

**Opción recomendada inicialmente: reutilizar el backend existente.**

Escribir en K++ el lexer, parser y lógica del compilador, aprovechando
en la primera etapa el backend, el formato de salida y las convenciones
que Stage 0 ya tiene. Esto reduce el alcance inicial y permite
concentrarse en demostrar el bootstrap.

La alternativa consiste en reescribir también el backend en K++. Ofrece
más independencia, pero introduce más trabajo y más puntos de fallo
antes de alcanzar la autocompilación.

La decisión debe revisarse cuando Stage 0 tenga una suite reproducible y
el subconjunto de Stage 1 esté definido.

------------------------------------------------------------------------

## Evolución multiplataforma

La prioridad actual es el target KolibriOS. A futuro, K++ podría
evolucionar hacia un compilador con varios targets, manteniendo el
frontend y la IR lo más independientes posible de la arquitectura.


### Concepto de target

Una interfaz futura podría permitir comandos como:

``` bash
kpp hello.cpp --target=kolibri-x86
kpp hello.cpp --target=linux-x86_64
kpp hello.cpp --target=windows-x86_64
kpp hello.cpp --target=linux-arm64
kpp hello.cpp --target=baremetal-arm64
```

Estos comandos son ejemplos de diseño futuro; no implican que las
opciones estén implementadas actualmente.

Cada target deberá describir, como mínimo:

-   **Arquitectura:** x86, x86-64, ARM64 u otra.
-   **Formato de salida:** Kolibri, ELF, PE o RAW.
-   **Runtime:** KolibriOS, Linux, Windows o bare metal.
-   **Convenciones de llamada:** argumentos, retornos, preservación de
    registros y pila.
-   **Operaciones soportadas:** capacidades y restricciones específicas
    del backend.

La abstracción de target permitiría evolucionar desde un compilador
centrado en KolibriOS hacia una arquitectura multiplataforma sin
confundir los objetivos futuros con las capacidades disponibles hoy.

------------------------------------------------------------------------

## Contribución y criterios de calidad

Para que el desarrollo siga siendo verificable:

1.  Añadir una prueba por cada característica o corrección importante.
2.  Separar pruebas del lexer, parser, semántica, IR, backend y
    ejecución final cuando sea posible.
3.  Ejecutar la suite de regresión después de modificar AST, scopes,
    allocator, llamadas o generación de código.
4.  Registrar los comandos ejecutados y sus resultados.
5.  Distinguir entre "implementado", "compila", "ejecuta correctamente"
    y "probado en KolibriOS".
6.  No marcar una fase como completa solo porque compile un ejemplo
    aislado.
7.  Mantener la documentación alineada con la implementación real.
8.  Para el bootstrap, guardar las fuentes y artefactos de cada etapa e
    indicar cómo reproducirlos.

## Visión del proyecto

```
K++ mínimo
    |
    v
Variables y expresiones
    |
    v
Control de flujo
    |
    v
Funciones y parámetros
    |
    v
Memoria y estructuras de datos
    |
    v
Runtime y ABI
    |
    v
Compilador escrito en K++
    |
    v
Bootstrap
    |
    v
Self-hosting
    |
    v
Nuevos targets y evolución del lenguaje
```

K++ se construye paso a paso: cada capacidad debe tener una
representación clara, una implementación coherente en las etapas del
compilador y pruebas que permitan verificarla. El objetivo no es solo
generar un ejecutable, sino desarrollar una herramienta de compilación
independiente, comprensible y extensible.
