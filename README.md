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
analyzer, IR, x86 backend and executable writer, with an 89-test
suite fully green. Stage 1 is the self-hosting rewrite in K++
itself: its lexer already compiles with Stage 0 and has run on
real KolibriOS (field-approved 2026-11-11). The language manual
(Spanish, v1.0) ships in the repo as `Manual_KPP.pdf`.

По-русски: K++ — родной компилятор для KolibriOS, переписываемый
на самом себе (self-hosting); руководство по языку — `Manual_KPP.pdf`.

## Estado

- **Stage 0 — completo.** El compilador escrito en C++ está
  consolidado y su batería pasa entera: 76 positivos,
  13 negativos, 0 fallos.
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

## En Windows

El Stage 0 es C++17 estándar, sin dependencias del sistema:
también se construye en Windows nativo, sin WSL. Hacen falta
git, CMake y un compilador C++ (el g++ de MinGW). Los tres
se instalan con winget:

```powershell
winget install --id Git.Git -e
winget install --id Kitware.CMake -e
winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e
```

Tras instalar, cierra y reabre la terminal: el PATH nuevo
solo se lee al abrirla. Después, el clon y la construcción
con CMake:

```powershell
git clone https://github.com/saykol31/KPP.git
cd KPP
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --parallel 2
```

Produce `bin\kpp.exe`. Con él se compila un programa K++
igual que en Linux:

```powershell
.\bin\kpp.exe projects\L01.02_hola.kpp
```

El ejecutable de KolibriOS queda en `Output\`, listo para
copiarlo a la imagen sin la extensión `.bin`.

Flujo probado en campo (2026-11-14): el `kpp.exe` construido
en Windows emitió para `L01.02_hola.kpp` un binario con el
mismo MD5 que el construido en Linux — el compilador es el
mismo en los dos sistemas, byte a byte.

Dos notas de campo (Windows 11):

- **Smart App Control** puede bloquear `kpp.exe` al
  ejecutarlo («An Application Control policy has blocked
  this file»): es la política de Windows contra programas
  sin firma. Desactivarla es decisión del usuario — está en
  Seguridad de Windows → Control de aplicaciones y
  navegador → Configuración de Smart App Control — y es de
  un solo sentido: no se reactiva sin reinstalar Windows.
- La consola puede mostrar torcidos los caracteres UTF-8
  de la salida del compilador; `chcp 65001` lo endereza.
  El binario generado no se ve afectado.

## Pruebas

```bash
./validar_test.sh
```

Compila la batería completa y comprueba que las pruebas
negativas sean rechazadas.
