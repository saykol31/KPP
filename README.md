# K++

Compilador del lenguaje K++ para KolibriOS. Traduce programas
`.kpp` a ejecutables nativos de KolibriOS con cabecera MENUET01:
sin intérprete ni máquina virtual, del fuente al binario
plano x86.

El compilador tiene frontend propio (lexer, parser, analizador
semántico), representación intermedia (IR), backend x86 y
escritor de ejecutables.

## In English

K++ is a native compiler for **KolibriOS**, the tiny x86 operating
system forked from MenuetOS. It translates `.kpp` source files into
flat native KolibriOS executables (MENUET01 header): no interpreter,
no virtual machine, straight from source to x86 binary. Stage 0,
written in C++, is complete — its own lexer, parser, semantic
analyzer, IR, x86 backend and executable writer, with a 71-test
suite fully green. Stage 1 is the self-hosting rewrite in K++
itself: its lexer already compiles with Stage 0 and has run on
real KolibriOS (field-approved 2026-11-11). The language manual
(Spanish, v1.0) ships in the repo as `Manual_KPP.pdf`.

По-русски: K++ — родной компилятор для KolibriOS, переписываемый
на самом себе (self-hosting); руководство по языку — `Manual_KPP.pdf`.

## Estado

- **Stage 0 — completo.** El compilador escrito en C++ está
  consolidado y su batería pasa entera: 60 positivos,
  11 negativos, 0 fallos.
- **Stage 1 — en curso.** El compilador reescrito en el propio
  K++. Pieza 1, el lexer, está en `compiler/kpp/lexer/`:
  compila con Stage 0 y su prueba de campo en KolibriOS está
  **aprobada** (2026-11-11: el Lexer corrió en KolibriOS desde
  AUTORUN y su salida coincidió con lo esperado — 27 tokens,
  0 errores).

## Estructura

- `src/`, `include/` — Stage 0, el compilador en C++.
- `compiler/kpp/` — Stage 1, el compilador en K++.
- `projects/` — la batería de pruebas, ordenada como creció
  el lenguaje: de `L00` a `L18`, con inserciones decimales
  y sufijo `_neg` para las pruebas negativas.
- `Manual_KPP.pdf` — el manual del lenguaje (v1.0).

## Construir

```bash
cmake -S . -B build
cmake --build build
```

Produce `bin/kpp`.

## Uso

```bash
./bin/kpp projects/L01_tipos.kpp
```

El ejecutable queda en `Output/`. Para correrlo en KolibriOS
se copia a la imagen **sin la extensión `.bin`**: KolibriOS
lo reconoce por su cabecera MENUET01, no por el nombre.

## Pruebas

```bash
./validar_test.sh
```

Compila la batería completa y comprueba que las pruebas
negativas sean rechazadas.
