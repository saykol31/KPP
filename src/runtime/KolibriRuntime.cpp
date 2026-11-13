#include "kpp/KolibriRuntime.hpp"
#include "kpp/X86Emitter.hpp"

#include <cstdint>

namespace kpp {

// ============================================================
// GRAPHICS RUNTIME
// ============================================================

KolibriRuntimeResult
KolibriRuntime::emitGraphicsRuntime(
    X86Emitter& emitter)
{
    KolibriRuntimeResult result;

    // ========================================================
    // PRINT CURSOR
    // ========================================================
    //
    // El cursor gráfico vive en una variable de DATA:
    // EBP queda libre como puntero de marco (Rebanada 1).
    //
    // X = 35
    // Y = 40
    //
    // KolibriOS recibe X/Y empaquetado en EBX.
    // El valor inicial se mantiene durante la ejecución
    // y cada PRINT / PRINT_INT avanza el cursor: Y baja
    // 10, y si pasa del fondo de la zona de texto
    // (Y > 440) vuelve a 40 y X salta una columna
    // (+110) — la ventana no tiene scroll y una sola
    // columna se quedaba corta.
    //

    emitter.emitMovEaxImm32(
        (35u << 16) + 40u);

    cursorPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

    // ========================================================
    // KONSOLE — MÁSCARA DE EVENTOS (rebanada 1)
    //
    // Función 40 de KolibriOS: el programa pide los
    // eventos que quiere recibir. 0x27 = REDRAW (0x01)
    // + KEY (0x02) + BUTTON (0x04) + MOUSE (0x20).
    // El ratón entra ya en la máscara: sus eventos
    // llegan al bucle y se consumen; el manejador de
    // desplazamiento llega en la rebanada 4.
    // ========================================================

    // mov eax, 40
    emitter.emitMovEaxImm32(40);

    // mov ebx, 0x27
    emitter.emitMovRegImm32(
        0xBB,
        0x27);

    // int 0x40
    emitter.emitInt(0x40);


    // ========================================================
    // START
    // ========================================================
    //
    // call drawWindow
    // jmp mainBody
    //
    // MAIN_BODY será generado posteriormente por X86Backend.
    //

    emitter.emitByte(0xE8);

    result.drawWindowCallPatch =
        emitter.emitRel32Placeholder();


    // JMP MAIN_BODY
    emitter.emitByte(0xE9);

    result.mainEntryPatch =
        emitter.emitRel32Placeholder();


    // ========================================================
    // EVENT LOOP
    // ========================================================

    const std::size_t eventLoop =
        emitter.size();

    result.eventLoopAddress =
        eventLoop;

    // mov eax, 10
    emitter.emitMovEaxImm32(10);

    // int 0x40
    emitter.emitInt(0x40);


    // ========================================================
    // EVENT 1 = REDRAW
    // ========================================================

    emitter.emitCmpEaxImm32(1);

    emitter.emitByte(0x0F);
    emitter.emitByte(0x84);

    const std::size_t redrawPatch =
        emitter.emitRel32Placeholder();


    // ========================================================
    // EVENT 2 = KEYBOARD
    // ========================================================

    emitter.emitCmpEaxImm32(2);

    emitter.emitByte(0x0F);
    emitter.emitByte(0x84);

    const std::size_t keyPatch =
        emitter.emitRel32Placeholder();


    // ========================================================
    // EVENT 3 = BUTTON
    // ========================================================

    emitter.emitCmpEaxImm32(3);

    emitter.emitByte(0x0F);
    emitter.emitByte(0x84);

    const std::size_t buttonPatch =
        emitter.emitRel32Placeholder();


    // ========================================================
    // EVENT 6 = MOUSE
    //
    // Konsole rebanada 1: el evento se reconoce y se
    // consume (vuelve al bucle). El desplazamiento
    // del historial se procesa en la rebanada 4.
    // ========================================================

    emitter.emitCmpEaxImm32(6);

    emitter.emitByte(0x0F);
    emitter.emitByte(0x84);

    const std::size_t mousePatch =
        emitter.emitRel32Placeholder();


    // ========================================================
    // EVENT DESCONOCIDO
    // ========================================================

    emitter.emitByte(0xE9);

    const std::size_t loopPatch =
        emitter.emitRel32Placeholder();


    // ========================================================
    // REDRAW
    // ========================================================

    const std::size_t redraw =
        emitter.size();

    // Redibujar ventana
    emitter.emitByte(0xE8);

    const std::size_t redrawWindowPatch =
        emitter.emitRel32Placeholder();

    /*
     * Después del redraw volvemos al EVENT_LOOP.
     *
     * El contenido NO se re-ejecuta: DRAW_WINDOW
     * repinta las líneas guardadas en el buffer de
     * líneas (cada PRINT anota la suya al imprimir).
     * Re-ejecutar MAIN_BODY volvía a correr el
     * programa entero sobre datos vivos: la salida
     * se pisaba y el estado compartido cambiaba
     * entre redibujos.
     */
    emitter.emitByte(0xE9);

    const std::size_t redrawLoopPatch =
        emitter.emitRel32Placeholder();


    // ========================================================
    // KEYBOARD
    // ========================================================

    const std::size_t key =
        emitter.size();

    // mov eax, 2
    emitter.emitMovEaxImm32(2);

    // int 0x40
    emitter.emitInt(0x40);

    // volver al event loop
    emitter.emitByte(0xE9);

    const std::size_t keyLoopPatch =
        emitter.emitRel32Placeholder();


    // ========================================================
    // BUTTON
    // ========================================================

    const std::size_t button =
        emitter.size();

    // mov eax, 17
    emitter.emitMovEaxImm32(17);

    // int 0x40
    emitter.emitInt(0x40);

    // KolibriOS devuelve el botón en AH
    emitter.emitCmpAhImm8(1);

    // Si no es botón 1 -> event loop
    emitter.emitByte(0x0F);
    emitter.emitByte(0x85);

    const std::size_t buttonNotClosePatch =
        emitter.emitRel32Placeholder();


    // ========================================================
    // EXIT
    // ========================================================

    emitExit(emitter);


    // ========================================================
    // DRAW WINDOW
    // ========================================================

    const std::size_t drawWindow =
        emitter.size();


    // --------------------------------------------------------
    // BEGIN REDRAW
    // --------------------------------------------------------

    emitter.emitMovEaxImm32(12);

    emitter.emitMovRegImm32(
        0xBB,
        1);

    emitter.emitInt(0x40);


    // --------------------------------------------------------
    // CREATE WINDOW
    // --------------------------------------------------------

    emitter.emitMovEaxImm32(0);

    // X = 100
    // WIDTH = 500
    emitter.emitMovRegImm32(
        0xBB,
        (100u << 16) + 500u);

    // Y = 100
    // HEIGHT = 220
    emitter.emitMovRegImm32(
        0xB9,
        (100u << 16) + 500u); // altura de 46 lineas

    // Work area color
    emitter.emitMovRegImm32(
        0xBA,
        0x14000000u);

    // Title color
    emitter.emitMovRegImm32(
        0xBE,
        0x808899FF);

    // EDI = title
    emitter.emitByte(0xBF);

    result.titleAddressPatch =
        emitter.emitImm32Placeholder();

    emitter.emitInt(0x40);


    // --------------------------------------------------------
    // REPAINT
    //
    // El contenido de la ventana no es persistente:
    // se repinta desde el buffer de líneas, dentro
    // del mismo begin/end redraw.
    // --------------------------------------------------------

    emitRepaint(emitter);


    // --------------------------------------------------------
    // TÍTULO — reafirmarlo con la función 71.1: en
    // los redibujos la función 0 pinta el marco pero
    // no el texto del título, y el header quedaba
    // borrado al mover la ventana.
    // --------------------------------------------------------

    // mov eax, 71
    emitter.emitMovEaxImm32(71);

    // mov ebx, 1 (subfunción: poner título)
    emitter.emitMovRegImm32(
        0xBB,
        1);

    // mov ecx, título
    emitter.emitByte(0xB9);

    result.titleCaptionPatch =
        emitter.emitImm32Placeholder();

    emitter.emitInt(0x40);


    // --------------------------------------------------------
    // END REDRAW
    // --------------------------------------------------------

    emitter.emitMovEaxImm32(12);

    emitter.emitMovRegImm32(
        0xBB,
        2);

    emitter.emitInt(0x40);

    emitter.emitRet();


    // ========================================================
    // PATCH INTERNAL RUNTIME JUMPS
    // ========================================================

    emitter.patchRel32(
        result.drawWindowCallPatch,
        drawWindow);

    emitter.patchRel32(
        redrawPatch,
        redraw);

    emitter.patchRel32(
        keyPatch,
        key);

    emitter.patchRel32(
        buttonPatch,
        button);

    emitter.patchRel32(
        loopPatch,
        eventLoop);

    emitter.patchRel32(
        redrawWindowPatch,
        drawWindow);

    emitter.patchRel32(
        redrawLoopPatch,
        eventLoop);

    emitter.patchRel32(
        keyLoopPatch,
        eventLoop);

    emitter.patchRel32(
        buttonNotClosePatch,
        eventLoop);

    emitter.patchRel32(
        mousePatch,
        eventLoop);


    return result;
}



// ============================================================
// FILE I/O RUNTIME — KOLIBRIOS
//
// KPP_FILE_IO_CYCLE1_BLOCK_RUNTIME
//
// Procesamiento de archivos mediante bloques de 64 KiB.
//
// WRITE:
//     SF70.2 una sola vez: crear/truncar archivo.
//     SF70.3 en cada iteración: escribir bloque.
//
// READ:
//     SF70.0 en cada iteración.
//
// Se conservan:
//     - ABI interna.
//     - Bloque de parámetros de 25 bytes.
//     - Scratch de 65536 bytes.
//     - Cuatro puntos de parche de fase 2.
//
// Registros:
//     ESI = base del bloque de parámetros.
//     EDI = base del scratch.
//
// Variables locales (marco reservado con SUB ESP, 24):
//
//     [ESP + 00] = total / done
//     [ESP + 04] = remaining
//     [ESP + 08] = chunk / bytes recibidos
//     [ESP + 12] = cursor del array int32_t
//     [ESP + 16] = indicador EOF (READ)
//     [ESP + 20] = tamaño solicitado del bloque (READ)
//
// ============================================================

KolibriFileIoRuntimeResult
KolibriRuntime::emitFileIoRuntime(
    X86Emitter& emitter)
{
    KolibriFileIoRuntimeResult result;

    using BranchList = std::vector<std::size_t>;

    constexpr std::uint32_t BLOCK =
        KOLIBRI_FILE_IO_MAX_BYTES;

    // --------------------------------------------------------
    // HELPERS DE EMISIÓN
    // --------------------------------------------------------

    const auto emitJcc =
        [&emitter](std::uint8_t condition)
        {
            emitter.emitByte(0x0F);
            emitter.emitByte(condition);
            return emitter.emitRel32Placeholder();
        };

    const auto emitJmp =
        [&emitter]()
        {
            emitter.emitByte(0xE9);
            return emitter.emitRel32Placeholder();
        };

    const auto emitCmpRegImm32 =
        [&emitter](
            std::uint8_t modrm,
            std::uint32_t value)
        {
            emitter.emitByte(0x81);
            emitter.emitByte(modrm);

            emitter.emitByte(
                static_cast<std::uint8_t>(value));

            emitter.emitByte(
                static_cast<std::uint8_t>(value >> 8));

            emitter.emitByte(
                static_cast<std::uint8_t>(value >> 16));

            emitter.emitByte(
                static_cast<std::uint8_t>(value >> 24));
        };

    const auto emitLoadEaxEsp =
        [&emitter](std::uint8_t displacement)
        {
            emitter.emitByte(0x8B);
            emitter.emitByte(0x44);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitLoadEcxEsp =
        [&emitter](std::uint8_t displacement)
        {
            emitter.emitByte(0x8B);
            emitter.emitByte(0x4C);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitLoadEdxEsp =
        [&emitter](std::uint8_t displacement)
        {
            emitter.emitByte(0x8B);
            emitter.emitByte(0x54);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitLoadEbxEsp =
        [&emitter](std::uint8_t displacement)
        {
            emitter.emitByte(0x8B);
            emitter.emitByte(0x5C);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitStoreEaxEsp =
        [&emitter](std::uint8_t displacement)
        {
            emitter.emitByte(0x89);
            emitter.emitByte(0x44);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitStoreEbxEsp =
        [&emitter](std::uint8_t displacement)
        {
            emitter.emitByte(0x89);
            emitter.emitByte(0x5C);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitStoreImmEsp =
        [&emitter](
            std::uint8_t displacement,
            std::uint32_t value)
        {
            emitter.emitByte(0xC7);
            emitter.emitByte(0x44);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);

            emitter.emitByte(
                static_cast<std::uint8_t>(value));

            emitter.emitByte(
                static_cast<std::uint8_t>(value >> 8));

            emitter.emitByte(
                static_cast<std::uint8_t>(value >> 16));

            emitter.emitByte(
                static_cast<std::uint8_t>(value >> 24));
        };

    const auto emitMovEsiImm32Placeholder =
        [&emitter]()
        {
            emitter.emitByte(0xBE);
            return emitter.emitImm32Placeholder();
        };

    const auto emitMovEdiImm32Placeholder =
        [&emitter]()
        {
            emitter.emitByte(0xBF);
            return emitter.emitImm32Placeholder();
        };

    const auto emitMovDwordEsiDispImm32 =
        [&emitter](
            std::uint8_t displacement,
            std::uint32_t value)
        {
            emitter.emitByte(0xC7);
            emitter.emitByte(0x46);
            emitter.emitByte(displacement);

            emitter.emitByte(
                static_cast<std::uint8_t>(value));

            emitter.emitByte(
                static_cast<std::uint8_t>(value >> 8));

            emitter.emitByte(
                static_cast<std::uint8_t>(value >> 16));

            emitter.emitByte(
                static_cast<std::uint8_t>(value >> 24));
        };

    const auto emitCommonPrologue =
        [&emitter]()
        {
            // Preservar registros callee-saved.
            emitter.emitPushEbx();
            emitter.emitByte(0x56); // PUSH ESI
            emitter.emitByte(0x57); // PUSH EDI

            // Guardar argumentos originales.
            emitter.emitPushEax();
            emitter.emitByte(0x51); // PUSH ECX
            emitter.emitByte(0x52); // PUSH EDX

            // Reservar 24 bytes de variables locales.
            emitter.emitByte(0x83);
            emitter.emitByte(0xEC);
            emitter.emitByte(0x18);
        };

    const auto emitCommonEpilogue =
        [&emitter]()
        {
            // Liberar variables locales.
            emitter.emitByte(0x83);
            emitter.emitByte(0xC4);
            emitter.emitByte(0x18);

            // Descartar argumentos guardados.
            emitter.emitByte(0x83);
            emitter.emitByte(0xC4);
            emitter.emitByte(0x0C);

            // Restaurar registros preservados.
            emitter.emitByte(0x5F); // POP EDI
            emitter.emitByte(0x5E); // POP ESI
            emitter.emitByte(0x5B); // POP EBX

            emitter.emitRet();
        };

    // ========================================================
    // FILE_WRITE
    // ========================================================

    result.fileWriteEntry =
        emitter.size();

    BranchList writeFailureBranches;
    BranchList writeSuccessBranches;

    emitCommonPrologue();

    /*
     * Distribución de pila:
     *
     * [ESP + 00] = total escrito
     * [ESP + 04] = bytes pendientes
     * [ESP + 08] = tamaño del bloque
     * [ESP + 12] = cursor del array
     * [ESP + 16] = reservado
     * [ESP + 20] = reservado
     *
     * [ESP + 24] = EDX original: capacidad
     * [ESP + 28] = ECX original: cantidad
     * [ESP + 32] = EAX original: ruta
     * [ESP + 36] = EDI preservado
     * [ESP + 40] = ESI preservado
     * [ESP + 44] = EBX preservado: array
     * [ESP + 48] = dirección de retorno
     */

    // --------------------------------------------------------
    // VALIDACIÓN DE ARGUMENTOS
    // --------------------------------------------------------

    // Ruta no nula.
    emitLoadEaxEsp(32);
    emitter.emitCmpEaxImm32(0);

    writeFailureBranches.push_back(
        emitJcc(0x84)); // JE

    // Array no nulo.
    emitLoadEaxEsp(44);
    emitter.emitCmpEaxImm32(0);

    writeFailureBranches.push_back(
        emitJcc(0x84)); // JE

    // Cantidad >= 0.
    emitLoadEcxEsp(28);
    emitCmpRegImm32(0xF9, 0);

    writeFailureBranches.push_back(
        emitJcc(0x8C)); // JL

    // Capacidad >= 0.
    emitLoadEdxEsp(24);
    emitCmpRegImm32(0xFA, 0);

    writeFailureBranches.push_back(
        emitJcc(0x8C)); // JL

    // Cantidad <= capacidad.
    emitLoadEcxEsp(28);
    emitLoadEdxEsp(24);

    emitter.emitByte(0x39);
    emitter.emitByte(0xD1); // CMP ECX, EDX

    writeFailureBranches.push_back(
        emitJcc(0x87)); // JA

    // --------------------------------------------------------
    // INICIALIZAR VARIABLES LOCALES
    // --------------------------------------------------------

    emitStoreImmEsp(0, 0);

    emitLoadEaxEsp(28);
    emitStoreEaxEsp(4);

    emitLoadEaxEsp(44);
    emitStoreEaxEsp(12);

    // --------------------------------------------------------
    // CARGAR BASES UNA SOLA VEZ
    // --------------------------------------------------------

    result.writeParamsAddressPatch =
        emitMovEsiImm32Placeholder();

    result.writeScratchAddressPatch =
        emitMovEdiImm32Placeholder();

    // --------------------------------------------------------
    // INICIALIZAR BLOQUE DE PARÁMETROS
    // --------------------------------------------------------

    // Subfunción inicial: CREATE / REWRITE = 2.
    emitMovDwordEsiDispImm32(0, 2);

    // Offset low.
    emitMovDwordEsiDispImm32(4, 0);

    // Offset high.
    emitMovDwordEsiDispImm32(8, 0);

    // Crear archivo vacío.
    emitMovDwordEsiDispImm32(12, 0);

    // Buffer nulo durante CREATE.
    emitMovDwordEsiDispImm32(16, 0);

    // Encoding RAW.
    emitter.emitByte(0xC6);
    emitter.emitByte(0x46);
    emitter.emitByte(0x14);
    emitter.emitByte(0);

    // Ruta.
    emitLoadEaxEsp(32);

    emitter.emitByte(0x89);
    emitter.emitByte(0x46);
    emitter.emitByte(0x15);

    // --------------------------------------------------------
    // SF70.2 — CREAR / REESCRIBIR UNA SOLA VEZ
    // --------------------------------------------------------

    emitter.emitByte(0x89);
    emitter.emitByte(0xF3); // MOV EBX, ESI

    emitter.emitMovEaxImm32(
        static_cast<std::int32_t>(
            KOLIBRI_FILE_IO_FUNCTION));

    emitter.emitInt(0x40);

    emitter.emitCmpEaxImm32(0);

    writeFailureBranches.push_back(
        emitJcc(0x85)); // JNE

    // ========================================================
    // BUCLE WRITE
    // ========================================================

    const std::size_t writeLoop =
        emitter.size();

    // Si remaining <= 0, terminar correctamente.
    emitLoadEaxEsp(4);

    emitter.emitByte(0x85);
    emitter.emitByte(0xC0); // TEST EAX, EAX

    const std::size_t writeFinishedBranch =
        emitJcc(0x8E); // JLE

    // --------------------------------------------------------
    // chunk = min(remaining, 65536)
    // --------------------------------------------------------

    emitLoadEaxEsp(4);

    emitCmpRegImm32(
        0xF8,
        BLOCK);

    const std::size_t writeUseRemaining =
        emitJcc(0x86); // JBE

    emitter.emitMovEaxImm32(
        static_cast<std::int32_t>(BLOCK));

    const std::size_t writeChunkSelected =
        emitter.size();

    emitter.patchRel32(
        writeUseRemaining,
        writeChunkSelected);

    emitStoreEaxEsp(8);

    // --------------------------------------------------------
    // CONVERSIÓN ARRAY int32_t -> SCRATCH
    // --------------------------------------------------------

    emitLoadEbxEsp(12);

    // EDX = cursor scratch.
    emitter.emitByte(0x89);
    emitter.emitByte(0xFA); // MOV EDX, EDI

    emitLoadEcxEsp(8);

    const std::size_t writeConvertLoop =
        emitter.size();

    // EAX = celda int32_t.
    emitter.emitByte(0x8B);
    emitter.emitByte(0x03); // MOV EAX, [EBX]

    // Guardar byte bajo.
    emitter.emitByte(0x88);
    emitter.emitByte(0x02); // MOV [EDX], AL

    // Array += 4.
    emitter.emitByte(0x83);
    emitter.emitByte(0xC3);
    emitter.emitByte(0x04);

    // Scratch += 1.
    emitter.emitByte(0x42); // INC EDX

    // ECX--.
    emitter.emitByte(0x49); // DEC ECX

    const std::size_t writeConvertBack =
        emitJcc(0x85); // JNZ

    emitter.patchRel32(
        writeConvertBack,
        writeConvertLoop);

    // Guardar cursor actualizado del array.
    emitStoreEbxEsp(12);

    // --------------------------------------------------------
    // PREPARAR SF70.3 — WRITE_FILE
    // --------------------------------------------------------

    // Subfunción WRITE_FILE = 3.
    emitMovDwordEsiDispImm32(0, 3);

    // OffsetLow = total escrito.
    emitLoadEaxEsp(0);

    emitter.emitByte(0x89);
    emitter.emitByte(0x46);
    emitter.emitByte(0x04);

    // OffsetHigh = 0.
    emitMovDwordEsiDispImm32(8, 0);

    // ByteCount = chunk.
    emitLoadEaxEsp(8);

    emitter.emitByte(0x89);
    emitter.emitByte(0x46);
    emitter.emitByte(0x0C);

    // BufferAddress = base scratch.
    emitter.emitByte(0x89);
    emitter.emitByte(0x7E);
    emitter.emitByte(0x10);

    // --------------------------------------------------------
    // INT 0x40 — SF70.3
    // --------------------------------------------------------

    emitter.emitByte(0x89);
    emitter.emitByte(0xF3); // MOV EBX, ESI

    emitter.emitMovEaxImm32(
        static_cast<std::int32_t>(
            KOLIBRI_FILE_IO_FUNCTION));

    emitter.emitInt(0x40);

    // EAX != 0 significa error.
    emitter.emitCmpEaxImm32(0);

    writeFailureBranches.push_back(
        emitJcc(0x85)); // JNE

    // --------------------------------------------------------
    // AVANZAR CONTADORES
    // --------------------------------------------------------

    // total += chunk.
    emitLoadEaxEsp(0);

    emitter.emitByte(0x03);
    emitter.emitByte(0x44);
    emitter.emitByte(0x24);
    emitter.emitByte(0x08);

    emitStoreEaxEsp(0);

    // remaining -= chunk.
    emitLoadEaxEsp(4);

    emitter.emitByte(0x2B);
    emitter.emitByte(0x44);
    emitter.emitByte(0x24);
    emitter.emitByte(0x08);

    emitStoreEaxEsp(4);

    // Siguiente bloque.
    const std::size_t writeLoopBack =
        emitJmp();

    emitter.patchRel32(
        writeLoopBack,
        writeLoop);

    // --------------------------------------------------------
    // WRITE COMPLETADO
    // --------------------------------------------------------

    const std::size_t writeSuccess =
        emitter.size();

    emitter.emitByte(0x31);
    emitter.emitByte(0xC0); // XOR EAX, EAX

    writeSuccessBranches.push_back(
        emitJmp());

    // --------------------------------------------------------
    // ERROR WRITE
    // --------------------------------------------------------

    const std::size_t writeFailure =
        emitter.size();

    emitter.emitMovEaxImm32(-1);

    // --------------------------------------------------------
    // EPÍLOGO WRITE
    // --------------------------------------------------------

    const std::size_t writeEpilogue =
        emitter.size();

    emitCommonEpilogue();

    emitter.patchRel32(
        writeFinishedBranch,
        writeSuccess);

    for (const std::size_t patch :
         writeFailureBranches)
    {
        emitter.patchRel32(
            patch,
            writeFailure);
    }

    for (const std::size_t patch :
         writeSuccessBranches)
    {
        emitter.patchRel32(
            patch,
            writeEpilogue);
    }

    // ========================================================
    // FILE_READ
    // ========================================================

    result.fileReadEntry =
        emitter.size();

    BranchList readFailureBranches;
    BranchList readSuccessBranches;

    emitCommonPrologue();

    /*
     * Variables locales:
     *
     * [ESP + 00] = total leído
     * [ESP + 04] = remaining
     * [ESP + 08] = chunk / bytes recibidos
     * [ESP + 12] = cursor array
     * [ESP + 16] = EOF
     * [ESP + 20] = tamaño solicitado del bloque
     *
     * Argumentos:
     *
     * [ESP + 24] = EDX original: capacidad
     * [ESP + 28] = ECX original: capacidad
     * [ESP + 32] = EAX original: ruta
     * [ESP + 44] = EBX original: array
     */

    // --------------------------------------------------------
    // VALIDACIÓN READ
    // --------------------------------------------------------

    // Ruta no nula.
    emitLoadEaxEsp(32);
    emitter.emitCmpEaxImm32(0);

    readFailureBranches.push_back(
        emitJcc(0x84)); // JE

    // Array no nulo.
    emitLoadEaxEsp(44);
    emitter.emitCmpEaxImm32(0);

    readFailureBranches.push_back(
        emitJcc(0x84)); // JE

    // Capacidad > 0.
    emitLoadEcxEsp(28);
    emitCmpRegImm32(0xF9, 0);

    readFailureBranches.push_back(
        emitJcc(0x8E)); // JLE

    // --------------------------------------------------------
    // INICIALIZAR CONTADORES
    // --------------------------------------------------------

    emitStoreImmEsp(0, 0);

    emitLoadEaxEsp(28);
    emitStoreEaxEsp(4);

    emitLoadEaxEsp(44);
    emitStoreEaxEsp(12);

    emitStoreImmEsp(16, 0);

    // --------------------------------------------------------
    // CARGAR BASES UNA SOLA VEZ
    // --------------------------------------------------------

    result.readParamsAddressPatch =
        emitMovEsiImm32Placeholder();

    result.readScratchAddressPatch =
        emitMovEdiImm32Placeholder();

    // --------------------------------------------------------
    // INICIALIZAR BLOQUE DE PARÁMETROS
    // --------------------------------------------------------

    // SF70.0 READ.
    emitMovDwordEsiDispImm32(0, 0);

    // OffsetLow.
    emitMovDwordEsiDispImm32(4, 0);

    // OffsetHigh.
    emitMovDwordEsiDispImm32(8, 0);

    // ByteCount.
    emitMovDwordEsiDispImm32(12, 0);

    // BufferAddress.
    emitter.emitByte(0x89);
    emitter.emitByte(0x7E);
    emitter.emitByte(0x10);

    // Encoding RAW.
    emitter.emitByte(0xC6);
    emitter.emitByte(0x46);
    emitter.emitByte(0x14);
    emitter.emitByte(0);

    // Ruta.
    emitLoadEaxEsp(32);

    emitter.emitByte(0x89);
    emitter.emitByte(0x46);
    emitter.emitByte(0x15);

    // ========================================================
    // BUCLE READ
    // ========================================================

    const std::size_t readLoop =
        emitter.size();

    // Si remaining <= 0, terminar.
    emitLoadEaxEsp(4);

    emitter.emitByte(0x85);
    emitter.emitByte(0xC0);

    const std::size_t readFinishedBranch =
        emitJcc(0x8E); // JLE

    // --------------------------------------------------------
    // chunk = min(remaining, 65536)
    // --------------------------------------------------------

    emitLoadEaxEsp(4);

    emitCmpRegImm32(
        0xF8,
        BLOCK);

    const std::size_t readUseRemaining =
        emitJcc(0x86); // JBE

    emitter.emitMovEaxImm32(
        static_cast<std::int32_t>(BLOCK));

    const std::size_t readChunkSelected =
        emitter.size();

    emitter.patchRel32(
        readUseRemaining,
        readChunkSelected);

    emitStoreEaxEsp(8);

    // Conservar también el tamaño solicitado en [ESP + 20]:
    // [ESP + 8] se sobrescribirá con los bytes recibidos
    // y la comparación de bloque corto necesita el pedido.
    emitStoreEaxEsp(20);

    // --------------------------------------------------------
    // ACTUALIZAR BLOQUE DE PARÁMETROS
    // --------------------------------------------------------

    // OffsetLow = total leído.
    emitLoadEaxEsp(0);

    emitter.emitByte(0x89);
    emitter.emitByte(0x46);
    emitter.emitByte(0x04);

    // OffsetHigh = 0.
    emitMovDwordEsiDispImm32(8, 0);

    // ByteCount = chunk.
    emitLoadEaxEsp(8);

    emitter.emitByte(0x89);
    emitter.emitByte(0x46);
    emitter.emitByte(0x0C);

    // BufferAddress permanece en scratch.

    // Reiniciar indicador EOF.
    emitStoreImmEsp(16, 0);

    // --------------------------------------------------------
    // INT 0x40 — SF70.0
    // --------------------------------------------------------

    emitter.emitByte(0x89);
    emitter.emitByte(0xF3); // MOV EBX, ESI

    emitter.emitMovEaxImm32(
        static_cast<std::int32_t>(
            KOLIBRI_FILE_IO_FUNCTION));

    emitter.emitInt(0x40);

    // --------------------------------------------------------
    // INTERPRETAR RESULTADO
    // --------------------------------------------------------

    // EAX == 6: EOF.
    emitter.emitCmpEaxImm32(
        static_cast<std::int32_t>(
            KOLIBRI_FILE_IO_EOF));

    const std::size_t readEofStatus =
        emitJcc(0x84); // JE

    // EAX == 0: lectura normal.
    emitter.emitCmpEaxImm32(0);

    const std::size_t readErrorStatus =
        emitJcc(0x85); // JNE

    // Saltar al procesamiento normal.
    const std::size_t readNormalStatus =
        emitJmp();

    // --------------------------------------------------------
    // EOF
    // --------------------------------------------------------

    const std::size_t readEofLabel =
        emitter.size();

    emitStoreImmEsp(16, 1);

    const std::size_t readEofContinue =
        emitJmp();

    // --------------------------------------------------------
    // ERROR DE SISTEMA
    // --------------------------------------------------------

    const std::size_t readError =
        emitter.size();

    // Si ya hay datos, devolver el total parcial.
    emitLoadEaxEsp(0);

    emitter.emitByte(0x85);
    emitter.emitByte(0xC0);

    const std::size_t readPartialBranch =
        emitJcc(0x85); // JNZ

    // Si no se leyó nada, devolver -1.
    const std::size_t readNoDataFailure =
        emitJmp();

    // --------------------------------------------------------
    // PROCESAMIENTO DE LECTURA
    // --------------------------------------------------------

    const std::size_t readProcess =
        emitter.size();

    // EBX contiene los bytes devueltos por SF70.0.
    // Validar que no excedan el bloque solicitado.
    emitLoadEaxEsp(8);

    // CMP EBX, EAX.
    emitter.emitByte(0x39);
    emitter.emitByte(0xC3);

    const std::size_t readInvalidCount =
        emitJcc(0x87); // JA

    // Guardar bytes realmente leídos.
    emitStoreEbxEsp(8);

    // Cero bytes: terminar sin entrar en conversión.
    emitter.emitByte(0x85);
    emitter.emitByte(0xDB); // TEST EBX, EBX

    const std::size_t readZeroBytes =
        emitJcc(0x84); // JE

    // --------------------------------------------------------
    // CONVERSIÓN SCRATCH -> ARRAY int32_t
    // --------------------------------------------------------

    // EDX = scratch base.
    emitter.emitByte(0x89);
    emitter.emitByte(0xFA);

    // EBX = cursor array.
    emitLoadEbxEsp(12);

    // ECX = bytes recibidos.
    emitLoadEcxEsp(8);

    const std::size_t readConvertLoop =
        emitter.size();

    // EAX = zero_extend(byte [EDX]).
    emitter.emitByte(0x0F);
    emitter.emitByte(0xB6);
    emitter.emitByte(0x02);

    // [EBX] = EAX.
    emitter.emitByte(0x89);
    emitter.emitByte(0x03);

    // Scratch += 1.
    emitter.emitByte(0x42);

    // Array += 4.
    emitter.emitByte(0x83);
    emitter.emitByte(0xC3);
    emitter.emitByte(0x04);

    // ECX--.
    emitter.emitByte(0x49);

    const std::size_t readConvertBack =
        emitJcc(0x85); // JNZ

    emitter.patchRel32(
        readConvertBack,
        readConvertLoop);

    // Guardar cursor array.
    emitStoreEbxEsp(12);

    // --------------------------------------------------------
    // ACTUALIZAR TOTAL Y REMAINING
    // --------------------------------------------------------

    // total += bytes recibidos.
    emitLoadEaxEsp(0);

    emitter.emitByte(0x03);
    emitter.emitByte(0x44);
    emitter.emitByte(0x24);
    emitter.emitByte(0x08);

    emitStoreEaxEsp(0);

    // remaining -= bytes recibidos.
    emitLoadEaxEsp(4);

    emitter.emitByte(0x2B);
    emitter.emitByte(0x44);
    emitter.emitByte(0x24);
    emitter.emitByte(0x08);

    emitStoreEaxEsp(4);

    // Si EOF, terminar.
    emitLoadEaxEsp(16);

    emitter.emitByte(0x85);
    emitter.emitByte(0xC0);

    const std::size_t readEofAfterCopy =
        emitJcc(0x85); // JNZ

    // Si bytes recibidos < tamaño solicitado ([ESP + 20]),
    // lectura corta: EOF.
    emitLoadEaxEsp(8);

    emitter.emitByte(0x3B);
    emitter.emitByte(0x44);
    emitter.emitByte(0x24);
    emitter.emitByte(0x14); // CMP EAX, [ESP + 20]

    const std::size_t readShortBlock =
        emitJcc(0x82); // JB

    // Bloque completo: siguiente iteración.
    const std::size_t readLoopBack =
        emitJmp();

    emitter.patchRel32(
        readLoopBack,
        readLoop);

    // --------------------------------------------------------
    // TERMINACIONES NORMALES
    // --------------------------------------------------------

    const std::size_t readSuccess =
        emitter.size();

    emitLoadEaxEsp(0);

    readSuccessBranches.push_back(
        emitJmp());

    // --------------------------------------------------------
    // ERROR SIN DATOS
    // --------------------------------------------------------

    const std::size_t readFailure =
        emitter.size();

    emitter.emitMovEaxImm32(-1);

    // --------------------------------------------------------
    // EPÍLOGO READ
    // --------------------------------------------------------

    const std::size_t readEpilogue =
        emitter.size();

    emitCommonEpilogue();

    // --------------------------------------------------------
    // RESOLVER SALTOS READ
    // --------------------------------------------------------

    emitter.patchRel32(
        readFinishedBranch,
        readSuccess);

    emitter.patchRel32(
        readEofStatus,
        readEofLabel);

    emitter.patchRel32(
        readEofContinue,
        readProcess);

    emitter.patchRel32(
        readNormalStatus,
        readProcess);

    emitter.patchRel32(
        readErrorStatus,
        readError);

    emitter.patchRel32(
        readPartialBranch,
        readSuccess);

    emitter.patchRel32(
        readNoDataFailure,
        readFailure);

    emitter.patchRel32(
        readInvalidCount,
        readError);

    emitter.patchRel32(
        readZeroBytes,
        readSuccess);

    emitter.patchRel32(
        readEofAfterCopy,
        readSuccess);

    emitter.patchRel32(
        readShortBlock,
        readSuccess);

    for (const std::size_t patch :
         readFailureBranches)
    {
        emitter.patchRel32(
            patch,
            readFailure);
    }

    for (const std::size_t patch :
         readSuccessBranches)
    {
        emitter.patchRel32(
            patch,
            readEpilogue);
    }

    return result;
}

// ============================================================
// FIN FILE I/O RUNTIME
// ============================================================


// ============================================================
// PRINT
// ============================================================

std::size_t
KolibriRuntime::emitPrint(
    X86Emitter& emitter,
    [[maybe_unused]] std::uint32_t y)
{
    // mov eax, 4
    emitter.emitMovEaxImm32(4);

    /*
     * KolibriOS:
     *
     * EBX = X << 16 + Y
     *
     * X = 35
     */
    // EBX = cursor gráfico actual (variable en DATA).
    cursorPatchList.push_back(
        emitter.emitMovEbxMem32Placeholder());

    // Text color
    emitter.emitMovRegImm32(
        0xB9,
        0x80FFFFFFu);

    // EDX = string
    emitter.emitByte(0xBA);

    const std::size_t textPatch =
        emitter.emitImm32Placeholder();

    // Text size
    emitter.emitMovRegImm32(
        0xBE,
        40);

    // int 0x40
    emitter.emitInt(0x40);

    // Anotar la línea en el buffer (EDX sigue en el
    // texto: la función 4 no lo toca).
    emitRecordLine(emitter, 40);

    // Avanzar el cursor: Y baja 10; si pasa del fondo
    // (440), Y vuelve a 40 y X salta una columna
    // (+110). Solo se toca EAX y la pila queda
    // balanceada en los dos caminos.
    cursorPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    emitter.emitAddEaxImm32(10);

    emitter.emitPushEax();

    emitter.emitAndEaxImm32(0x0000FFFF);

    emitter.emitCmpEaxImm32(440);

    const std::size_t sinSalto =
        emitter.emitJbeRel32Placeholder();

    emitter.emitPopEax();

    emitter.emitAndEaxImm32(
        static_cast<std::int32_t>(0xFFFF0000u));

    emitter.emitAddEaxImm32(40);

    emitter.emitAddEaxImm32(110u << 16);

    const std::size_t saltoFin =
        emitter.emitJmpRel32Placeholder();

    const std::size_t sigue =
        emitter.size();

    emitter.emitPopEax();

    const std::size_t fin =
        emitter.size();

    emitter.patchRel32(sinSalto, sigue);

    emitter.patchRel32(saltoFin, fin);

    cursorPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

    return textPatch;
}


std::size_t
KolibriRuntime::emitPrintFromEdx(
    X86Emitter& emitter,
    [[maybe_unused]] std::uint32_t y)
{
    // mov eax, 4
    emitter.emitMovEaxImm32(4);

    // EBX = cursor gráfico actual (variable en DATA).
    cursorPatchList.push_back(
        emitter.emitMovEbxMem32Placeholder());

    // Text color
    emitter.emitMovRegImm32(
        0xB9,
        0x80FFFFFFu);

    // EDX ya contiene la dirección del string.

    // Text size
    emitter.emitMovRegImm32(
        0xBE,
        40);

    // int 0x40
    emitter.emitInt(0x40);

    // Anotar la línea en el buffer.
    emitRecordLine(emitter, 40);

    // Avanzar el cursor: Y baja 10; si pasa del fondo
    // (440), Y vuelve a 40 y X salta una columna
    // (+110). Solo se toca EAX y la pila queda
    // balanceada en los dos caminos.
    cursorPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    emitter.emitAddEaxImm32(10);

    emitter.emitPushEax();

    emitter.emitAndEaxImm32(0x0000FFFF);

    emitter.emitCmpEaxImm32(440);

    const std::size_t sinSalto =
        emitter.emitJbeRel32Placeholder();

    emitter.emitPopEax();

    emitter.emitAndEaxImm32(
        static_cast<std::int32_t>(0xFFFF0000u));

    emitter.emitAddEaxImm32(40);

    emitter.emitAddEaxImm32(110u << 16);

    const std::size_t saltoFin =
        emitter.emitJmpRel32Placeholder();

    const std::size_t sigue =
        emitter.size();

    emitter.emitPopEax();

    const std::size_t fin =
        emitter.size();

    emitter.patchRel32(sinSalto, sigue);

    emitter.patchRel32(saltoFin, fin);

    cursorPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

    return 0;
}


// ============================================================
// EXIT
// ============================================================

std::size_t
KolibriRuntime::emitPrintInteger(
    X86Emitter& emitter,
    [[maybe_unused]] std::uint32_t y)
{
    /*
     * EAX contiene el entero calculado por K++.
     *
     * Convertimos el número a ASCII escribiendo
     * los dígitos desde el final del buffer hacia atrás.
     *
     * Ejemplo:
     *
     *     EAX = 30
     *
     *     30 / 10 -> resto 0 -> '0'
     *      3 / 10 -> resto 3 -> '3'
     *
     * Resultado:
     *
     *     "30\0"
     */

    // El RETURN posterior necesita recuperar EAX y EBX:
    // la rutina carga la posición del texto en EBX (SF 4)
    // y un temporal puede residir en EBX al imprimir; sin
    // preservarlo, un RETURN posterior desde EBX devolvía
    // la posición del texto en lugar del valor.
    emitter.emitPushEax();
    emitter.emitPushEbx();

    /*
     * EDI apunta inicialmente al final del buffer.
     *
     * MOV EDI, buffer_address
     *
     * El backend parcheará el imm32.
     */
    const std::size_t bufferPatch =
        emitter.size() + 1;

    emitter.emitMovEdiImm32(0);

    /*
     * Dejamos espacio para el terminador NUL.
     */
    emitter.emitDecEdi();
    emitter.emitMovEdiMem8Imm8(0);

    /*
     * Caso especial: número 0.
     */
    emitter.emitCmpEaxImm32(0);

    const std::size_t zeroJump =
        emitter.emitJeRel32Placeholder();

    /*
     * Conversión decimal.
     */
    emitter.emitXorEdxEdx();
    emitter.emitMovEcxImm32(10);

    const std::size_t conversionLoop =
        emitter.size();

    // EDX:EAX / ECX
    // EDX debe ser cero antes de cada DIV.
    emitter.emitXorEdxEdx();
    emitter.emitDivEcx();

    // resto 0..9 -> ASCII
    emitter.emitAddDlImm8('0');

    // Retroceder en el buffer.
    emitter.emitDecEdi();

    // Guardar carácter.
    emitter.emitMovEdiMem8Dl();

    // ¿Quedan más dígitos?
    emitter.emitCmpEaxImm32(0);

    const std::size_t loopJump =
        emitter.emitJneRel32Placeholder();

    // Los números distintos de cero saltan el caso especial.
    const std::size_t endZeroJump =
        emitter.emitJmpRel32Placeholder();

    /*
     * Caso EAX == 0.
     */
    const std::size_t zeroLabel =
        emitter.size();

    emitter.emitDecEdi();
    emitter.emitMovEdiMem8Imm8('0');

    const std::size_t endZeroLabel =
        emitter.size();

    /*
     * Parcheamos los saltos.
     */
    emitter.patchRel32(
        zeroJump,
        zeroLabel);

    emitter.patchRel32(
        loopJump,
        conversionLoop);

    emitter.patchRel32(
        endZeroJump,
        endZeroLabel);

    /*
     * Syscall KolibriOS:
     *
     * EAX = 4
     * EBX = posición X/Y
     * ECX = color
     * EDX = puntero al texto
     * ESI = tamaño máximo
     */
    emitter.emitMovEdxEdi();

    emitter.emitMovEaxImm32(4);

    // EBX = cursor gráfico actual (variable en DATA).
    cursorPatchList.push_back(
        emitter.emitMovEbxMem32Placeholder());

    emitter.emitMovRegImm32(
        0xB9,
        0x80FFFFFFu);

    emitter.emitMovRegImm32(
        0xBE,
        16);

    emitter.emitInt(0x40);

    // Anotar la línea en el buffer: el texto son los
    // dígitos a los que EDI apunta; EDX = EDI para la
    // rutina común de anotación.
    emitter.emitMovEdxEdi();

    emitRecordLine(emitter, 16);

    // Avanzar el cursor: Y baja 10; si pasa del fondo
    // (440), Y vuelve a 40 y X salta una columna
    // (+110). Solo se toca EAX y la pila queda
    // balanceada en los dos caminos.
    cursorPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    emitter.emitAddEaxImm32(10);

    emitter.emitPushEax();

    emitter.emitAndEaxImm32(0x0000FFFF);

    emitter.emitCmpEaxImm32(440);

    const std::size_t sinSalto =
        emitter.emitJbeRel32Placeholder();

    emitter.emitPopEax();

    emitter.emitAndEaxImm32(
        static_cast<std::int32_t>(0xFFFF0000u));

    emitter.emitAddEaxImm32(40);

    emitter.emitAddEaxImm32(110u << 16);

    const std::size_t saltoFin =
        emitter.emitJmpRel32Placeholder();

    const std::size_t sigue =
        emitter.size();

    emitter.emitPopEax();

    const std::size_t fin =
        emitter.size();

    emitter.patchRel32(sinSalto, sigue);

    emitter.patchRel32(saltoFin, fin);

    cursorPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

    /*
     * Restaurar el resultado original.
     */
    emitter.emitPopEbx();
    emitter.emitPopEax();

    return bufferPatch;
}

// ============================================================
// BUFFER DE LÍNEAS — ANOTAR
//
// Cada PRINT, tras dibujar su línea, la anota en la
// siguiente ranura del buffer:
//
//   ranura (64 bytes) = [longitud de dibujo (4)]
//                       [40 bytes de texto]
//
// Se copian los mismos 40 bytes que la función 4
// dibuja desde el puntero del texto (en el entero se
// guardan con longitud 16, la suya: el repintado solo
// redibuja esa longitud). Con el buffer lleno (128
// líneas) se sigue imprimiendo; solo se deja de
// anotar. Al entrar: EDX = texto. Se tocan EAX, ECX,
// ESI y EDI — muertos tras el dibujo en los tres
// emisores de impresión — y la pila no se toca.
// ============================================================

void
KolibriRuntime::emitRecordLine(
    X86Emitter& emitter,
    std::uint32_t drawLength)
{
    // KONSOLE — registro circular (rebanada 2).
    //
    // La línea se escribe en la ranura lineHead;
    // la cabeza avanza y al llegar a 512 vuelve
    // a 0: la ranura más vieja se reutiliza y la
    // salida nunca se detiene. lineCount cuenta
    // las líneas vivas y se queda en 512. Al
    // entrar, EDX apunta al texto. Se tocan EAX,
    // ECX, ESI y EDI, como en la versión lineal
    // que esto sustituye.

    // EAX = lineHead (la próxima ranura física).
    lineHeadPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    // EAX = ranura * 64 + base del buffer.
    // shl eax, 6
    emitter.emitByte(0xC1);
    emitter.emitByte(0xE0);
    emitter.emitByte(0x06);

    // add eax, imm32 (la base)
    emitter.emitByte(0x05);

    lineBufferPatchList.push_back(
        emitter.emitImm32Placeholder());

    // mov esi, eax  (ESI = ranura)
    emitter.emitByte(0x89);
    emitter.emitByte(0xC6);

    // mov dword [esi], drawLength
    emitter.emitByte(0xC7);
    emitter.emitByte(0x06);

    const std::size_t lenPatch =
        emitter.emitImm32Placeholder();

    emitter.patch32(lenPatch, drawLength);

    // mov edi, esi
    emitter.emitByte(0x89);
    emitter.emitByte(0xF7);

    // add edi, 4
    emitter.emitByte(0x83);
    emitter.emitByte(0xC7);
    emitter.emitByte(0x04);

    // Copiar 40 bytes: [EDX] -> [EDI].
    emitter.emitMovEcxImm32(40);

    const std::size_t copia =
        emitter.size();

    // mov al, [edx]
    emitter.emitByte(0x8A);
    emitter.emitByte(0x02);

    // mov [edi], al
    emitter.emitByte(0x88);
    emitter.emitByte(0x07);

    // inc edx / inc edi / dec ecx
    emitter.emitByte(0x42);
    emitter.emitByte(0x47);
    emitter.emitByte(0x49);

    const std::size_t copiaJump =
        emitter.emitJneRel32Placeholder();

    emitter.patchRel32(copiaJump, copia);

    // lineHead++ y vuelta a 0 al llegar a 512.
    lineHeadPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    emitter.emitAddEaxImm32(1);

    emitter.emitCmpEaxImm32(512);

    // jb guardar
    emitter.emitByte(0x0F);
    emitter.emitByte(0x82);

    const std::size_t sinVueltaPatch =
        emitter.emitRel32Placeholder();

    // mov eax, 0
    emitter.emitMovEaxImm32(0);

    const std::size_t guardar =
        emitter.size();

    emitter.patchRel32(sinVueltaPatch, guardar);

    lineHeadPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

    // lineCount: sube hasta 512 y ahí se queda.
    lineCountPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    emitter.emitCmpEaxImm32(512);

    // jae fin
    emitter.emitByte(0x0F);
    emitter.emitByte(0x83);

    const std::size_t llenoPatch =
        emitter.emitRel32Placeholder();

    emitter.emitAddEaxImm32(1);

    lineCountPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

    const std::size_t fin =
        emitter.size();

    emitter.patchRel32(llenoPatch, fin);
}


// ============================================================
// BUFFER DE LÍNEAS — REPINTAR
//
// Se emite dentro de DRAW_WINDOW, entre la función 0
// y el fin del redibujo. Recorre el buffer por índice
// (EDI) y redibuja cada línea guardada con su longitud
// propia, avanzando el cursor DE REPINTADO — su propia
// celda, con la misma regla de columnas de los PRINT —
// desde (35, 40). El cursor de impresión y el cuerpo
// del programa no se tocan: repintar es repetir la
// página desde el estado, nunca continuarla.
// ============================================================

void
KolibriRuntime::emitRepaint(
    X86Emitter& emitter)
{
    // cursorRepintado = (35, 40)
    emitter.emitMovEaxImm32(
        (35u << 16) + 40u);

    repaintCursorPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

    // EDI = índice de línea.
    emitter.emitMovEdiImm32(0);

    const std::size_t bucle =
        emitter.size();

    // ¿Índice >= líneas? -> fin.
    lineCountPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    // cmp eax, edi
    emitter.emitByte(0x39);
    emitter.emitByte(0xF8);

    const std::size_t finPatch =
        emitter.emitJbeRel32Placeholder();

    // EAX = índice * 64 + base del buffer.
    // mov eax, edi
    emitter.emitByte(0x89);
    emitter.emitByte(0xF8);

    // shl eax, 6
    emitter.emitByte(0xC1);
    emitter.emitByte(0xE0);
    emitter.emitByte(0x06);

    // add eax, imm32 (la base)
    emitter.emitByte(0x05);

    lineBufferPatchList.push_back(
        emitter.emitImm32Placeholder());

    // mov edx, eax  (EDX = ranura)
    emitter.emitByte(0x89);
    emitter.emitByte(0xC2);

    // mov esi, [eax]  (ESI = longitud de dibujo)
    emitter.emitByte(0x8B);
    emitter.emitByte(0x30);

    // add edx, 4  (EDX = texto)
    emitter.emitByte(0x83);
    emitter.emitByte(0xC2);
    emitter.emitByte(0x04);

    // EBX = cursor de repintado.
    repaintCursorPatchList.push_back(
        emitter.emitMovEbxMem32Placeholder());

    // ECX = color
    emitter.emitMovRegImm32(
        0xB9,
        0x80FFFFFFu);

    // Dibujar la línea.
    emitter.emitMovEaxImm32(4);

    emitter.emitInt(0x40);

    // Avanzar el cursor de repintado: la misma
    // secuencia de columnas que usan los PRINT,
    // sobre su propia celda.
    repaintCursorPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    emitter.emitAddEaxImm32(10);

    emitter.emitPushEax();

    emitter.emitAndEaxImm32(0x0000FFFF);

    emitter.emitCmpEaxImm32(440);

    const std::size_t sinSalto =
        emitter.emitJbeRel32Placeholder();

    emitter.emitPopEax();

    emitter.emitAndEaxImm32(
        static_cast<std::int32_t>(0xFFFF0000u));

    emitter.emitAddEaxImm32(40);

    emitter.emitAddEaxImm32(110u << 16);

    const std::size_t saltoFin =
        emitter.emitJmpRel32Placeholder();

    const std::size_t sigue =
        emitter.size();

    emitter.emitPopEax();

    const std::size_t avanceFin =
        emitter.size();

    emitter.patchRel32(sinSalto, sigue);

    emitter.patchRel32(saltoFin, avanceFin);

    repaintCursorPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

    // inc edi
    emitter.emitByte(0x47);

    const std::size_t otraPatch =
        emitter.emitJmpRel32Placeholder();

    emitter.patchRel32(otraPatch, bucle);

    const std::size_t fin =
        emitter.size();

    emitter.patchRel32(finPatch, fin);
}


// ============================================================
// ACCESORES DEL BUFFER DE LÍNEAS
// ============================================================

const std::vector<std::size_t>&
KolibriRuntime::lineBufferPatches() const
{
    return lineBufferPatchList;
}

const std::vector<std::size_t>&
KolibriRuntime::lineCountPatches() const
{
    return lineCountPatchList;
}

const std::vector<std::size_t>&
KolibriRuntime::lineHeadPatches() const
{
    return lineHeadPatchList;
}


const std::vector<std::size_t>&
KolibriRuntime::firstVisiblePatches() const
{
    return firstVisiblePatchList;
}

const std::vector<std::size_t>&
KolibriRuntime::repaintCursorPatches() const
{
    return repaintCursorPatchList;
}


void
KolibriRuntime::emitExit(
    X86Emitter& emitter)
{
    // mov eax, -1
    emitter.emitMovEaxImm32(-1);

    // int 0x40
    emitter.emitInt(0x40);
}


// ============================================================
// CURSOR GRÁFICO — KPP_CURSOR_DATA
// ============================================================

const std::vector<std::size_t>&
KolibriRuntime::cursorPatches() const
{
    return cursorPatchList;
}

}
