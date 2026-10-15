#!/usr/bin/env bash
#
# validar_test.sh — regresión automática de K++.
# Descubre archivos .kpp en projects/ y compiler/kpp/.
#
# Uso:
#   ./validar_test.sh
#   ./validar_test.sh --no-build

set -uo pipefail
cd "$(dirname "$0")" || exit 1

if [[ "${1:-}" != "--no-build" ]]; then
    echo "=== Reconstruyendo bin/kpp ==="
    if ! cmake --build build --target kpp; then
        echo "FAIL: compilación del compilador"
        exit 1
    fi
    echo
fi

if [[ ! -x ./bin/kpp ]]; then
    echo "FAIL: ./bin/kpp no existe o no es ejecutable"
    exit 1
fi

# Solo pruebas conocidas que deben fallar.
# Las rutas son relativas a la raíz del repositorio.
NEGATIVOS=(
    "projects/test_l14_1_error_sintactico.kpp"
    "projects/test_l14_2_variable_no_declarada.kpp"
    "projects/test_l14_3_tipos_incompatibles.kpp"
    "projects/test_l14_4_asignacion_incompatible.kpp"
    "projects/test_enum_invalido.kpp"
    "projects/test_struct_duplicado.kpp"
    "projects/test_struct_campo_duplicado.kpp"
    "projects/function_parameters_bad_count.kpp"
    "projects/function_parameters_bad_extra.kpp"
    "projects/test_l13_struct_campo_inexistente.kpp"

)

declare -A ES_NEGATIVO=()
for t in "${NEGATIVOS[@]}"; do
    ES_NEGATIVO["$t"]=1
done

mapfile -d '' TODOS < <(
    find projects compiler/kpp -type f -name '*.kpp' -print0 2>/dev/null |
    sort -z
)

if [[ ${#TODOS[@]} -eq 0 ]]; then
    echo "FAIL: no se encontraron archivos .kpp"
    exit 1
fi

PASS=0
FAIL=0
NEG_OK=0
NEG_FAIL=0
FALLIDOS=()
OMITIDOS=0
TMP_LOG="$(mktemp)"
trap 'rm -f "$TMP_LOG"' EXIT

echo "=== Pruebas positivas y negativas ==="

for ruta in "${TODOS[@]}"; do
    # Normalizar la ruta para poder comparar con la lista.
    t="${ruta#./}"

    if [[ -n "${ES_NEGATIVO[$t]+x}" ]]; then
        if [[ ! -f "$t" ]]; then
            printf 'ERROR: falta test negativo declarado: %s\n' "$t"
            FAIL=$((FAIL + 1))
            FALLIDOS+=("$t (archivo negativo ausente)")
            continue
        fi

        if ./bin/kpp "$t" >"$TMP_LOG" 2>&1; then
            printf 'FAIL-NEG  %s (debía fallar, pero compiló)\n' "$t"
            NEG_FAIL=$((NEG_FAIL + 1))
            FALLIDOS+=("$t (debía fallar)")
        else
            printf 'PASS-NEG  %s (rechazado correctamente)\n' "$t"
            NEG_OK=$((NEG_OK + 1))
        fi
        continue
    fi

    if ./bin/kpp "$t" >"$TMP_LOG" 2>&1; then
        printf 'PASS      %s\n' "$t"
        PASS=$((PASS + 1))
    else
        printf 'FAIL      %s\n' "$t"
        tail -4 "$TMP_LOG" | sed 's/^/          /'
        FAIL=$((FAIL + 1))
        FALLIDOS+=("$t")
    fi
done

# Verificar que las exclusiones declaradas existen y fueron descubiertas.
for t in "${NEGATIVOS[@]}"; do
    encontrado=0
    for ruta in "${TODOS[@]}"; do
        if [[ "${ruta#./}" == "$t" ]]; then
            encontrado=1
            break
        fi
    done
    if [[ $encontrado -eq 0 ]]; then
        printf 'ERROR: test negativo declarado no encontrado: %s\n' "$t"
        FAIL=$((FAIL + 1))
        FALLIDOS+=("$t (no encontrado)")
    fi
done

echo
echo "========================================"
echo "PASS positivos : $PASS"
echo "PASS negativos : $NEG_OK"
echo "FAIL           : $((FAIL + NEG_FAIL))"
echo "Total evaluado : $((PASS + NEG_OK + FAIL + NEG_FAIL))"
echo "Archivos .kpp  : ${#TODOS[@]}"

if [[ $FAIL -gt 0 || $NEG_FAIL -gt 0 ]]; then
    echo
    echo "Fallos:"
    printf '  - %s\n' "${FALLIDOS[@]}"
    exit 1
fi

echo "OK: todas las pruebas de compilación pasaron."
