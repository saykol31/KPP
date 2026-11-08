#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace kpp {

// ============================================================
// KPP_FILE_IO_RUNTIME_CONTRACT_PHASE1
//
// CONTRATO DE E/S DE ARCHIVOS — KOLIBRIOS
//
// Este bloque define exclusivamente la ABI de parámetros
// que utilizarán las rutinas x86 del runtime.
//
// La emisión de las rutinas y la reserva del scratch
// se implementarán en la fase 2.
//
// KolibriOS:
//     EAX = 70
//     EBX = dirección del bloque
//     INT 0x40
//
// Subfunciones:
//     0 = READ
//     2 = WRITE / CREATE / REWRITE
//
// Scratch:
//     65536 bytes.
//
// Cada celda del array K++ representa un byte.
// ============================================================

inline constexpr std::uint32_t
    KOLIBRI_FILE_IO_FUNCTION = 70u;

inline constexpr std::uint32_t
    KOLIBRI_FILE_IO_SUBFUNCTION_READ = 0u;

inline constexpr std::uint32_t
    KOLIBRI_FILE_IO_SUBFUNCTION_WRITE = 2u;

inline constexpr std::uint32_t
    KOLIBRI_FILE_IO_MAX_BYTES = 64u * 1024u;

inline constexpr std::uint8_t
    KOLIBRI_FILE_IO_ENCODING_RAW = 0u;

inline constexpr std::int32_t
    KOLIBRI_FILE_IO_SUCCESS = 0;

inline constexpr std::int32_t
    KOLIBRI_FILE_IO_FAILURE = -1;


// ============================================================
// BLOQUE DE PARÁMETROS DE LA FUNCIÓN 70
//
// IMPORTANTE:
//
// La estructura está empaquetada.
//
// El campo pathAddress comienza en el desplazamiento +21.
// No debe introducirse padding entre encodingFlag
// y pathAddress.
//
// Todos los campos de dirección son uint32_t porque
// el código generado para KolibriOS es x86 de 32 bits.
//
// Distribución:
//
//     +00 uint32_t subfunction
//     +04 uint32_t offsetLow
//     +08 uint32_t offsetHigh
//     +12 uint32_t byteCount
//     +16 uint32_t bufferAddress
//     +20 uint8_t  encodingFlag
//     +21 uint32_t pathAddress
//
// Tamaño total: 25 bytes.
// ============================================================

#pragma pack(push, 1)

struct KolibriFileIoParams
{
    std::uint32_t subfunction = 0;

    std::uint32_t offsetLow = 0;

    std::uint32_t offsetHigh = 0;

    std::uint32_t byteCount = 0;

    std::uint32_t bufferAddress = 0;

    std::uint8_t encodingFlag =
        KOLIBRI_FILE_IO_ENCODING_RAW;

    std::uint32_t pathAddress = 0;
};

#pragma pack(pop)


// ============================================================
// VERIFICACIÓN ESTÁTICA DE LA ABI
//
// Estas comprobaciones impiden que un cambio accidental
// de alineación altere el bloque esperado por KolibriOS.
// ============================================================

static_assert(
    sizeof(KolibriFileIoParams) == 25,
    "KolibriFileIoParams debe ocupar exactamente 25 bytes"
);

static_assert(
    offsetof(KolibriFileIoParams, subfunction) == 0,
    "Offset incorrecto: subfunction"
);

static_assert(
    offsetof(KolibriFileIoParams, offsetLow) == 4,
    "Offset incorrecto: offsetLow"
);

static_assert(
    offsetof(KolibriFileIoParams, offsetHigh) == 8,
    "Offset incorrecto: offsetHigh"
);

static_assert(
    offsetof(KolibriFileIoParams, byteCount) == 12,
    "Offset incorrecto: byteCount"
);

static_assert(
    offsetof(KolibriFileIoParams, bufferAddress) == 16,
    "Offset incorrecto: bufferAddress"
);

static_assert(
    offsetof(KolibriFileIoParams, encodingFlag) == 20,
    "Offset incorrecto: encodingFlag"
);

static_assert(
    offsetof(KolibriFileIoParams, pathAddress) == 21,
    "Offset incorrecto: pathAddress"
);


// ============================================================
// CONTRATO SEMÁNTICO DEL RUNTIME
//
// kpp_file_write(ruta, arreglo, cantidad, capacidad)
//
//     ruta:
//         Cadena terminada en NUL.
//
//     arreglo:
//         Dirección del array de celdas int32_t.
//
//     cantidad:
//         Número de celdas que se escribirán.
//
//     capacidad:
//         Número de celdas disponibles en el array.
//
//     Validaciones:
//         cantidad >= 0
//         cantidad <= capacidad
//         cantidad <= KOLIBRI_FILE_IO_MAX_BYTES
//         capacidad <= KOLIBRI_FILE_IO_MAX_BYTES
//
//     Conversión:
//         Una celda K++ produce un byte.
//
//     Retorno:
//          0 = éxito
//         -1 = error.
//
//     Una cantidad inválida no debe tocar el disco.
//
// ------------------------------------------------------------
//
// kpp_file_read(ruta, arreglo, capacidad)
//
//     ruta:
//         Cadena terminada en NUL.
//
//     arreglo:
//         Dirección del array de celdas int32_t.
//
//     capacidad:
//         Número máximo de bytes que se pueden recibir.
//
//     Validaciones:
//         capacidad > 0
//         capacidad <= KOLIBRI_FILE_IO_MAX_BYTES
//
//     Lectura:
//         Nunca solicitar más bytes que la capacidad.
//
//     Conversión:
//         Cada byte leído se almacena en una celda int32_t.
//
//     Retorno:
//         >= 0 = cantidad de bytes leídos.
//         -1   = error.
//
// ------------------------------------------------------------
//
// BLOQUE DE PARÁMETROS:
//
//     subfunction:
//         0 para lectura.
//         2 para escritura.
//
//     offsetLow / offsetHigh:
//         0 para operaciones desde el comienzo del archivo.
//
//     byteCount:
//         Cantidad solicitada.
//
//     bufferAddress:
//         Dirección del scratch de transferencia.
//
//     encodingFlag:
//         KOLIBRI_FILE_IO_ENCODING_RAW.
//
//     pathAddress:
//         Dirección absoluta de la ruta terminada en NUL.
//
// La reserva del scratch y el parcheo de direcciones
// se implementarán en la fase 2.
// ============================================================


class X86Emitter;

struct KolibriRuntimeResult
{
    // CALL DRAW_WINDOW / START
    std::size_t drawWindowCallPatch = 0;

    // JMP MAIN_BODY desde START
    std::size_t mainEntryPatch = 0;

    // JMP MAIN_BODY después de REDRAW
    std::size_t redrawMainPatch = 0;

    // Dirección relativa dentro del código
    std::size_t eventLoopAddress = 0;

    // Dirección del título
    std::size_t titleAddressPatch = 0;
};

class KolibriRuntime
{
public:

    /*
     * Genera la infraestructura gráfica de KolibriOS:
     *
     * START
     *   CALL DRAW_WINDOW
     *   JMP MAIN_BODY
     *
     * EVENT_LOOP
     *   REDRAW
     *   KEYBOARD
     *   BUTTON
     *
     * DRAW_WINDOW
     */
    KolibriRuntimeResult
    emitGraphicsRuntime(
        X86Emitter& emitter);

    /*
     * Genera una instrucción PRINT de K++.
     *
     * Devuelve la posición que posteriormente
     * será parcheada con la dirección del texto.
     */
    std::size_t
    emitPrint(
        X86Emitter& emitter,
        std::uint32_t y);

    std::size_t
    emitPrintFromEdx(
        X86Emitter& emitter,
        std::uint32_t y);
    /*
     * Imprime el entero actualmente contenido en EAX.
     *
     * Devuelve el offset del imm32 que contiene
     * la dirección del buffer decimal.
     */
    std::size_t
    emitPrintInteger(
        X86Emitter& emitter,
        std::uint32_t y);

    /*
     * Genera salida inmediata de KolibriOS.
     */
    void
    emitExit(
        X86Emitter& emitter);

    /*
     * Posiciones del código generado que referencian
     * la variable del cursor gráfico en DATA.
     *
     * El backend las parchea con la dirección
     * absoluta de la variable cuando conoce dataBase.
     *
     * KPP_CURSOR_DATA
     */
    const std::vector<std::size_t>&
    cursorPatches() const;

private:

    // Cursor gráfico: X/Y empaquetado en una variable
    // de DATA. Antes vivía en EBP; EBP es ahora el
    // puntero de marco de las funciones (Rebanada 1).
    std::vector<std::size_t> cursorPatchList;
};

}
