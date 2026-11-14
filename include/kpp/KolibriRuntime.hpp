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

// KolibriOS puede devolver 6 al alcanzar EOF.
inline constexpr std::uint32_t
    KOLIBRI_FILE_IO_EOF = 6u;

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
//         cantidad y capacidad sin límite total: el
//         runtime transfiere en bloques internos de
//         hasta KOLIBRI_FILE_IO_MAX_BYTES bytes
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
//         (sin límite total: el runtime lee en bloques
//         internos de hasta KOLIBRI_FILE_IO_MAX_BYTES)
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
//         offsetLow = posición del bloque actual (bytes
//         ya transferidos; 0 en el primer bloque) y
//         offsetHigh = 0. En escritura, primero se crea
//         o trunca el archivo con la subfunción 2 (cero
//         bytes) y cada bloque de datos se escribe con
//         la subfunción 3 (archivo existente, con offset).
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


// ============================================================
// KPP_FILE_IO_RUNTIME_PHASE2
//
// ABI INTERNA DE LAS RUTINAS FILE I/O
//
// WRITE:
//
//     EAX = dirección de la ruta
//     EBX = dirección del array int32_t
//     ECX = cantidad de celdas
//     EDX = capacidad del array
//
// READ:
//
//     EAX = dirección de la ruta
//     EBX = dirección del array int32_t
//     ECX = capacidad del array
//
// RETORNO:
//
//     EAX = resultado.
//
// REGISTROS:
//
//     EAX, ECX, EDX: volátiles.
//     EBX, ESI, EDI: preservados por el runtime.
//     EBP, ESP: preservados.
//
// La rutina guarda los registros preservados y los argumentos
// que necesita en su propio marco de pila.
//
// La pila queda exactamente restaurada antes de RET.
//
// El caller debe preservar los temporales vivos que puedan
// resultar destruidos durante la preparación de argumentos,
// siguiendo la disciplina L15.
//
// SCRATCH:
//
//     65536 bytes compartidos entre READ y WRITE.
//
// BLOQUE:
//
//     sizeof(KolibriFileIoParams) = 25 bytes.
//
// EOF:
//
//     El código 6 de KolibriOS es éxito corto: EBX trae
//     los bytes realmente leídos y la rutina devuelve
//     ese conteo, igual que con EAX == 0.
// ============================================================

class X86Emitter;

struct KolibriRuntimeResult
{
    // CALL DRAW_WINDOW / START
    std::size_t drawWindowCallPatch = 0;

    // JMP MAIN_BODY desde START
    std::size_t mainEntryPatch = 0;

    // Dirección relativa dentro del código
    std::size_t eventLoopAddress = 0;

    // Dirección del título
    std::size_t titleAddressPatch = 0;

    // Segunda referencia al título: la función 71.1
    // lo reafirma en cada dibujo (en los redibujos la
    // función 0 no vuelve a pintar su texto).
    std::size_t titleCaptionPatch = 0;

    // Konsole (rebanada 3): dirección de la rutina
    // REFRESH_OUTPUT. El backend parchea con ella
    // las llamadas que los PRINT emiten tras
    // registrar su línea.
    std::size_t refreshOutputAddress = 0;
};


// ============================================================
// RESULTADO DE EMISIÓN DEL RUNTIME FILE I/O
//
// Las posiciones son offsets dentro del código x86.
// El backend las parchea cuando conoce dataBase.
// ============================================================

struct KolibriFileIoRuntimeResult
{
    std::size_t fileWriteEntry = 0;
    std::size_t fileReadEntry = 0;

    std::size_t writeScratchAddressPatch = 0;
    std::size_t writeParamsAddressPatch = 0;

    std::size_t readScratchAddressPatch = 0;
    std::size_t readParamsAddressPatch = 0;
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
     * Genera las rutinas x86 internas de FILE_WRITE y FILE_READ.
     *
     * Devuelve:
     *     Entradas de ambas rutinas.
     *     Parches para scratch.
     *     Parches para el bloque de parámetros.
     *
     * Las direcciones absolutas se resolverán cuando
     * X86Backend conozca la dirección base de DATA.
     */
    KolibriFileIoRuntimeResult
    emitFileIoRuntime(
        X86Emitter& emitter);

    /*
     * Genera salida inmediata de KolibriOS.
     */
    void
    emitExit(
        X86Emitter& emitter);

    // Posiciones que referencian la base del buffer de
    // líneas, su contador y el cursor de repintado.
    // El backend las parchea igual que las del cursor.
    const std::vector<std::size_t>&
    lineBufferPatches() const;

    const std::vector<std::size_t>&
    lineCountPatches() const;

    // Konsole: la cabeza circular de escritura y la
    // primera línea visible (esta última se llena
    // desde la rebanada 3; en la 2 solo se reserva).
    const std::vector<std::size_t>&
    lineHeadPatches() const;

    const std::vector<std::size_t>&
    firstVisiblePatches() const;

    const std::vector<std::size_t>&
    repaintCursorPatches() const;

    // Konsole (rebanada 3): posiciones de las
    // llamadas de los PRINT a REFRESH_OUTPUT.
    const std::vector<std::size_t>&
    refreshCallPatches() const;

    // Konsole (rebanada 4): la geometría de la barra
    // y el estado del arrastre del ratón.
    const std::vector<std::size_t>&
    thumbTopPatches() const;

    const std::vector<std::size_t>&
    thumbHeightPatches() const;

    const std::vector<std::size_t>&
    dragStatePatches() const;

    const std::vector<std::size_t>&
    dragGrabPatches() const;

    // Konsole (rebanada EDIT): el estado del menu y
    // de la seleccion, el portapapeles de la consola
    // y el bloque de textos del menu.
    const std::vector<std::size_t>&
    menuAbiertoPatches() const;

    const std::vector<std::size_t>&
    seleccionPatches() const;

    const std::vector<std::size_t>&
    clipLenPatches() const;

    const std::vector<std::size_t>&
    clipBufferPatches() const;

    const std::vector<std::size_t>&
    menuTextoPatches() const;

private:

    // Konsole (rebanada 2): anota la línea en el
    // buffer CIRCULAR de 512 registros, en la ranura
    // lineHead; la cabeza avanza y al llegar a 512
    // vuelve a 0 (la ranura más vieja se reutiliza)
    // y lineCount se queda en 512. Al entrar, EDX
    // apunta al texto y la longitud de dibujo es la
    // que usó la función 4 (40 en textos, 16 en
    // enteros).
    void
    emitRecordLine(
        X86Emitter& emitter,
        std::uint32_t drawLength);

    // Konsole (rebanada 3): dibuja las líneas
    // visibles del buffer circular en vertical —
    // X = 35, Y = 40 + fila * 10 — desde
    // firstVisible y hasta 41 líneas. Es una rutina
    // con RET: la llaman DRAW_WINDOW (tras la
    // función 0) y REFRESH_OUTPUT (tras limpiar el
    // área de texto con la función 13). El cuerpo
    // del programa no se re-ejecuta nunca.
    // Rebanada 4: el dibujo se cierra llamando a la
    // barra de desplazamiento; devuelve la posición
    // del parche de esa llamada.
    std::size_t
    emitDrawLines(
        X86Emitter& emitter);

    // Konsole (rebanada 3): emite la rutina
    // REFRESH_OUTPUT — begin redraw, limpieza del
    // área de texto (función 13), llamada a la
    // rutina de dibujo, end redraw, RET. Los PRINT
    // la llaman tras registrar su línea. Devuelve
    // la posición del parche de su llamada interna
    // a la rutina de dibujo.
    std::size_t
    emitRefreshOutput(
        X86Emitter& emitter);

    // Konsole (rebanada 4): emite la rutina de la
    // barra de desplazamiento — carril y pulgar con
    // la función 13, en X = 478, Y = 40, alto 410.
    // El pulgar es proporcional al historial
    // (alto = 410 * min(lineCount, 41) / lineCount)
    // y su posición sigue a firstVisible. Guarda la
    // geometría en las celdas thumbTop/thumbHeight
    // para el manejador del ratón. Rutina con RET.
    void
    emitScrollbar(
        X86Emitter& emitter);

    // Konsole (rebanada 4): emite el manejador del
    // evento 6 en el runtime gráfico. Consulta
    // botones y coordenadas con la función 37:
    // clic sobre el carril pagina arriba o abajo
    // (41 líneas), clic sobre el pulgar lo arrastra
    // con desplazamiento proporcional, y soltar el
    // botón cancela el arrastre. Si firstVisible
    // cambia, llama a DRAW_WINDOW (su parche es lo
    // que devuelve); si no, vuelve al bucle sin
    // repintar. eventLoop ya se conoce al emitirlo
    // y sus saltos de salida se parchean dentro.
    // Konsole (rebanada EDIT): dibuja el boton EDIT
    // (funcion 8, id 2) en la barra de titulo y, si
    // el menu esta abierto, el panel con sus cinco
    // renglones como botones 3 a 7: CORTAR, COPIAR,
    // SELECCIONAR TODO, PEGAR y ELIMINAR — el menu
    // EDIT de TinyPad. La llaman DRAW_WINDOW y
    // REFRESH_OUTPUT: el refresco limpia el area de
    // texto y lo que el menu pinta encima debe
    // repintarse.
    void
    emitDibujaEdit(
        X86Emitter& emitter);

    // Konsole (rebanada EDIT): copia las lineas del
    // historial al portapapeles — de cada registro,
    // el texto hasta su primer NUL o su longitud de
    // dibujo, con las lineas unidas por CR LF como
    // en el portapapeles del sistema — y lo pone
    // tambien en ese portapapeles con la funcion
    // 54.2, con su encabezado de 12 bytes (tamano
    // total, tipo 0 = texto, codificacion 1 =
    // CP866), como put_to_clipboard de TinyPad.
    void
    emitCopiar(
        X86Emitter& emitter);

    // Konsole (rebanada EDIT): pega el portapapeles
    // en KONSOLE.TXT, en la raiz, con la funcion 70
    // (subfuncion 2: crear y escribir). Con el
    // portapapeles vacio (clipLen = 0) no escribe.
    void
    emitPegar(
        X86Emitter& emitter);

    // Konsole (rebanada EDIT): vacia el historial
    // — lineCount, lineHead y firstVisible a cero
    // y la seleccion se apaga. Es el cuerpo de
    // ELIMINAR y la segunda mitad de CORTAR.
    void
    emitVaciar(
        X86Emitter& emitter);

    std::size_t
    emitMouseHandler(
        X86Emitter& emitter,
        std::size_t eventLoop);

    std::vector<std::size_t> lineBufferPatchList;
    std::vector<std::size_t> lineCountPatchList;
    std::vector<std::size_t> lineHeadPatchList;
    std::vector<std::size_t> firstVisiblePatchList;
    std::vector<std::size_t> repaintCursorPatchList;
    std::vector<std::size_t> refreshCallPatchList;
    std::vector<std::size_t> thumbTopPatchList;
    std::vector<std::size_t> thumbHeightPatchList;
    std::vector<std::size_t> dragStatePatchList;
    std::vector<std::size_t> dragGrabPatchList;
    std::vector<std::size_t> menuAbiertoPatchList;
    std::vector<std::size_t> seleccionPatchList;
    std::vector<std::size_t> clipLenPatchList;
    std::vector<std::size_t> clipBufferPatchList;
    std::vector<std::size_t> menuTextoPatchList;
    std::vector<std::size_t> editCallPatchList;
};

}
