# K++

Compilador del lenguaje K++ para KolibriOS. Traduce programas
`.kpp` a ejecutables nativos de KolibriOS con cabecera MENUET01:
sin intérprete y sin runtime externo, del fuente al binario
plano x86.

El compilador tiene frontend propio (lexer, parser, analizador
semántico), representación intermedia (IR), backend x86 y
escritor de ejecutables.

## Estado

- **Stage 0 — completo.** El compilador escrito en C++ está
  consolidado y su batería pasa entera: 60 positivos,
  11 negativos, 0 fallos.
- **Stage 1 — en curso.** El compilador reescrito en el propio
  K++. Pieza 1, el lexer, está en `compiler/kpp/lexer/`:
  compila con Stage 0; su prueba de campo en KolibriOS está
  pendiente.

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
