# Batería de pruebas del compilador K++

Este documento registra las pruebas del compilador K++, sus objetivos,
los resultados esperados y su estado de validación.

## Estados

- APROBADA: compilación y comportamiento esperado verificados.
- COMPILA, PENDIENTE DE VALIDAR: hay evidencia de compilación, pero falta
  confirmar el comportamiento esperado.
- PENDIENTE: todavía no se ha comprobado la prueba.
- NEGATIVA: el código debe ser rechazado por contener un error intencional.
- ERROR: el resultado obtenido no coincide con el esperado.

## Pruebas de llamadas a funciones y parámetros

| Archivo | Objetivo | Resultado esperado | Estado |
|---|---|---|---|
| test_function_call_variables.kpp | Pasar variables como argumentos | Retorno 30 | COMPILA, PENDIENTE DE VALIDAR |
| test_function_call_expression.kpp | Usar una expresión en un argumento | Retorno 35 | COMPILA, PENDIENTE DE VALIDAR |
| test_function_call_two_expressions.kpp | Usar expresiones en ambos argumentos | Retorno 55 | COMPILA, PENDIENTE DE VALIDAR |
| test_function_call_forward.kpp | Llamar a una función definida después de main | Imprimir 20 | COMPILA, PENDIENTE DE VALIDAR |
| test_function_call_store.kpp | Guardar el retorno en una variable | Compilar y terminar con retorno 0 | COMPILA, PENDIENTE DE VALIDAR |
| test_function_call_print.kpp | Imprimir el retorno de una llamada | Imprimir 10 | COMPILA, PENDIENTE DE VALIDAR |
| test_function_call_noprint.kpp | Retornar directamente el resultado de una llamada | Retorno 10 | COMPILA, PENDIENTE DE VALIDAR |
| test_function_parameters_use.kpp | Usar parámetros en una suma | Retorno 30 | COMPILA, PENDIENTE DE VALIDAR |
| test_una_sola_llamada.kpp | Llamada simple con dos argumentos | Imprimir 30 | COMPILA, PENDIENTE DE VALIDAR |

## Reglas de validación

1. No marcar una prueba como aprobada únicamente porque existan archivos
   `.bin` y `.raw`.
2. Registrar los errores de compilación en las pruebas negativas como
   resultados esperados cuando corresponda.
3. No confundir el código de retorno del programa con el resultado de
   la compilación.
4. Conservar los archivos fuente de referencia en `projects/` hasta
   clasificar las pruebas restantes.
5. Mantener los archivos generados en `Output/` y `build/` fuera de los
   commits de organización de pruebas, salvo decisión explícita.
