# Stage 1, pieza 1 — lexer en K++, rebanada 1

Borrador preparado 2026-11-09 por Muse (sin compilar en la MX todavía;
compilado en sandbox, ver abajo). No es un commit ni un push: los
archivos son para que Esequiel los coloque y los pruebe en campo.

## Archivos

- `Lexer.kpp` — el lexer, todo en `main()`, ~430 líneas. Lee
  `FUENTE.KPP` una sola vez con `file_read` y vuelca por token
  `(tipo, inicio, largo)`, códigos según `TokenType` de Stage 0.
- `FUENTE.KPP` — fuente de prueba (109 bytes): `int`, identificador,
  número, `//` comentario, `if`, `==`, string, `return`.
- `EXPECTED.txt` — salida exacta esperada: `TOKENS`, 27 ternas,
  `27`, `0`.

## Qué cubre la rebanada 1

Identificadores y las 9 palabras clave, números, strings (sin escapes,
igual que Stage 0), `//` comentarios, y los 25 operadores/puntuación
de un carácter o dos (`== != <= >= && || ::` y el resto). No cubre:
`&` y `|` sueltos ni `:` suelta (Stage 0 los rechaza con error; aquí
se cuentan en `errores`).

## Por qué está escrito así (verificado contra Stage 0)

- Todo en `main()`: K++ no pasa arreglos por parámetro (L17) y un
  `return` de temporal auto-imprime (corrección 2026-11-08).
- `&&` NO cortocircuita: baja a una sola AND con los dos lados
  evaluados. Por eso ningún `fuente[pos]` va a la derecha de un `&&`;
  los guardas de fin de fuente van en `if` separados y los escaneos
  usan un flag `sigue` (no existe `break`).
- Sin literales de carácter en K++: se comparan códigos numéricos.
- Las palabras clave se clasifican con `if`s anidados guardados por
  `largo`, para no leer nunca fuera del identificador escaneado.

## Comprobación hecha en sandbox (2026-11-09)

- El compilador Stage 0 se construyó limpio en una copia `/tmp`
  del clon (3b49a39) + el salto `//` replicado del commit b35d47c.
- `Lexer.kpp` compila: exit 0, `Lexer.bin` de 83.109 bytes.
- `FUENTE.KPP` compila: exit 0 (es K++ válido).
- Salida esperada simulada en Python con las mismas reglas del
  borrador: 27 tokens, 0 errores.

## Pasos en la MX (campo, por él)

1. Copiar `Lexer.kpp` a `~/C/KPP/compiler/kpp/lexer/Lexer.kpp`
   (sustituye el stub de 4 líneas; git lo tiene cubierto).
2. Compilar con Stage 0: `./bin/kpp compiler/kpp/lexer/Lexer.kpp`
   (el compilador de la MX ya está en b35d47c, con `//`).
3. Inyectar `Lexer.bin` con asm_env.sh opción 8, y `FUENTE.KPP`
   en la raíz de la imagen con:
   `mcopy -o -i "<imagen>@@1048576" FUENTE.KPP ::/FUENTE.KPP`
   (mismo patrón que usa el script; verificar con `mdir` que
   quedó en `::/`).
4. Arrancar en QEMU y comparar la salida con `EXPECTED.txt`.
   Aceptación: `27` y `0` al final, ternas idénticas.

Nota: en la Surface la opción 8 sigue bloqueada por la imagen
persistente ausente (vive en la MX); esta prueba es en la MX,
donde la imagen ya está.
