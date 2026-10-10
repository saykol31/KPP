# Batería de pruebas del compilador K++

Este documento registra las pruebas del compilador K++, sus objetivos,
los resultados esperados y su estado de validación.

## Estados

- APROBADA: el resultado esperado se ha comprobado mediante la revisión
  del código generado y del desensamblado.
- PENDIENTE: todavía no se ha comprobado la prueba.
- NEGATIVA: el código debe ser rechazado por contener un error intencional.
- ERROR: el resultado obtenido no coincide con el esperado.

## Pruebas de llamadas a funciones y parámetros

| Archivo | Objetivo | Resultado esperado | Estado |
|---|---|---|---|
| test_function_call_variables.kpp | Pasar variables como argumentos | Retorno 30 | APROBADA |
| test_function_call_expression.kpp | Usar una expresión en un argumento | Retorno 35 | APROBADA |
| test_function_call_two_expressions.kpp | Usar expresiones en ambos argumentos | Retorno 55 | APROBADA |
| test_function_call_forward.kpp | Llamar a una función definida después de main | Imprimir 20 | APROBADA |
| test_function_call_store.kpp | Guardar el retorno en una variable | Retorno 0, sin impresión explícita | APROBADA |
| test_function_call_print.kpp | Imprimir el retorno de una llamada | Imprimir 10 | APROBADA |
| test_function_call_noprint.kpp | Retornar directamente el resultado de una llamada | Retorno 10 | APROBADA |
| test_function_parameters_use.kpp | Usar parámetros en una suma | Retorno 30 | APROBADA |
| test_una_sola_llamada.kpp | Llamada simple con dos argumentos | Imprimir 30 | APROBADA |

## Comprobaciones realizadas

En las nueve pruebas se verificó que el ejecutable contiene una cabecera
`MENUET01` válida y que el desensamblado corresponde al resultado esperado.

Se revisaron, entre otros aspectos:

- Paso de argumentos y operaciones aritméticas.
- Cálculo de expresiones dentro de los argumentos.
- Resolución de llamadas a funciones definidas más adelante.
- Almacenamiento y retorno de valores.
- Generación de las secuencias de impresión.
- Balance de las instrucciones `call` y `ret`.
- Ausencia de instrucciones privilegiadas inesperadas; `int 0x40` se utiliza
  como mecanismo de llamadas al sistema de KolibriOS.

## Comportamiento diagnóstico de Stage 0

**Importante:** en `IRGenerator.cpp` existe un comportamiento diagnóstico
que genera una instrucción `PrintInteger` antes de ciertos retornos de
expresiones no constantes.

Esto puede hacer que un valor aparezca en pantalla aunque la fuente no
contenga una llamada explícita a impresión. Por tanto, al analizar el
desensamblado, hay que distinguir entre:

- La impresión solicitada explícitamente por el programa.
- La impresión añadida por el compilador como ayuda de diagnóstico.

Este comportamiento se considera parte del funcionamiento actual de Stage 0,
no una prueba de que el programa fuente haya solicitado imprimir ese valor.
Conviene documentarlo para evitar confusiones durante las pruebas.

### Mejora futura

Añadir una opción del compilador para activar o desactivar las impresiones
diagnósticas, manteniendo el comportamiento actual de Stage 0 hasta que
se decida formalmente el cambio.

## Reglas de validación

1. No considerar suficiente la existencia de archivos `.bin` y `.raw`.
2. Registrar los errores de compilación de las pruebas negativas como
   resultados esperados cuando corresponda.
3. Distinguir el código de retorno del programa del resultado de compilación.
4. Mantener las pruebas organizadas en `compiler/kpp/`.
5. Mantener los archivos generados de `Output/` y `build/` fuera de los
   commits de documentación y pruebas, salvo decisión explícita.
