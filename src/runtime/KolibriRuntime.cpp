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
    // y cada PRINT / PRINT_INT avanza Y.
    //

    emitter.emitMovEaxImm32(
        (35u << 16) + 40u);

    cursorPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

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
     * Después del redraw volvemos a ejecutar MAIN_BODY.
     *
     * Esto permite que los PRINT vuelvan a aparecer
     * si KolibriOS solicita redibujar la ventana.
     */
    emitter.emitByte(0xE9);

    result.redrawMainPatch =
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
        keyLoopPatch,
        eventLoop);

    emitter.patchRel32(
        buttonNotClosePatch,
        eventLoop);


    return result;
}



// ============================================================
// FILE I/O RUNTIME — KOLIBRIOS
// ============================================================

KolibriFileIoRuntimeResult
KolibriRuntime::emitFileIoRuntime(
    X86Emitter& emitter)
{
    KolibriFileIoRuntimeResult result;

    using BranchList = std::vector<std::size_t>;

    // --------------------------------------------------------
    // HELPERS DE CODIFICACIÓN X86
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
            // MOV EAX, [ESP + disp8]
            emitter.emitByte(0x8B);
            emitter.emitByte(0x44);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitLoadEcxEsp =
        [&emitter](std::uint8_t displacement)
        {
            // MOV ECX, [ESP + disp8]
            emitter.emitByte(0x8B);
            emitter.emitByte(0x4C);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitLoadEdxEsp =
        [&emitter](std::uint8_t displacement)
        {
            // MOV EDX, [ESP + disp8]
            emitter.emitByte(0x8B);
            emitter.emitByte(0x54);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitLoadEsiEsp =
        [&emitter](std::uint8_t displacement)
        {
            // MOV ESI, [ESP + disp8]
            emitter.emitByte(0x8B);
            emitter.emitByte(0x74);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitLoadEdiEsp =
        [&emitter](std::uint8_t displacement)
        {
            // MOV EDI, [ESP + disp8]
            emitter.emitByte(0x8B);
            emitter.emitByte(0x7C);
            emitter.emitByte(0x24);
            emitter.emitByte(displacement);
        };

    const auto emitMovEbxImm32Placeholder =
        [&emitter]()
        {
            // MOV EBX, imm32
            emitter.emitByte(0xBB);
            return emitter.emitImm32Placeholder();
        };

    const auto emitMovEsiImm32Placeholder =
        [&emitter]()
        {
            // MOV ESI, imm32
            emitter.emitByte(0xBE);
            return emitter.emitImm32Placeholder();
        };

    const auto emitMovEdiImm32Placeholder =
        [&emitter]()
        {
            // MOV EDI, imm32
            emitter.emitByte(0xBF);
            return emitter.emitImm32Placeholder();
        };

    const auto emitMovDwordEbxDispImm32 =
        [&emitter](
            std::uint8_t displacement,
            std::uint32_t value)
        {
            // MOV DWORD [EBX + disp8], imm32
            emitter.emitByte(0xC7);
            emitter.emitByte(0x43);
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

            // Guardar argumentos.
            emitter.emitPushEax();
            emitter.emitByte(0x51); // PUSH ECX
            emitter.emitByte(0x52); // PUSH EDX
        };

    const auto emitCommonEpilogue =
        [&emitter]()
        {
            // Descartar EDX, ECX y EAX guardados.
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
     * Distribución de pila después del prólogo:
     *
     * [ESP + 00] = EDX original: capacidad
     * [ESP + 04] = ECX original: cantidad
     * [ESP + 08] = EAX original: ruta
     * [ESP + 12] = EDI preservado
     * [ESP + 16] = ESI preservado
     * [ESP + 20] = EBX preservado: array
     * [ESP + 24] = dirección de retorno
     */

    // Ruta no nula.
    emitLoadEaxEsp(8);
    emitter.emitCmpEaxImm32(0);

    writeFailureBranches.push_back(
        emitJcc(0x84)); // JE

    // Array no nulo.
    emitLoadEaxEsp(20);
    emitter.emitCmpEaxImm32(0);

    writeFailureBranches.push_back(
        emitJcc(0x84)); // JE

    // Cantidad >= 0.
    emitLoadEcxEsp(4);
    emitCmpRegImm32(0xF9, 0); // CMP ECX, 0

    writeFailureBranches.push_back(
        emitJcc(0x8C)); // JL

    // Capacidad >= 0.
    emitLoadEdxEsp(0);
    emitCmpRegImm32(0xFA, 0); // CMP EDX, 0

    writeFailureBranches.push_back(
        emitJcc(0x8C)); // JL

    // Capacidad <= 65536.
    emitCmpRegImm32(
        0xFA,
        KOLIBRI_FILE_IO_MAX_BYTES);

    writeFailureBranches.push_back(
        emitJcc(0x87)); // JA

    // Cantidad <= capacidad.
    emitLoadEcxEsp(4);
    emitLoadEdxEsp(0);

    emitter.emitByte(0x39);
    emitter.emitByte(0xD1); // CMP ECX, EDX

    writeFailureBranches.push_back(
        emitJcc(0x87)); // JA

    // --------------------------------------------------------
    // CONVERSIÓN ARRAY int32_t -> SCRATCH byte[]
    // --------------------------------------------------------

    emitLoadEsiEsp(20);

    result.writeScratchAddressPatch =
        emitMovEdiImm32Placeholder();

    // EDX conserva la base del scratch.
    //
    // El bucle de conversión avanza EDI, así que la
    // dirección que necesita el campo bufferAddress
    // del bloque es la base, no el puntero avanzado.
    // EDX ya no interviene tras las validaciones
    // (cantidad y capacidad viven en la pila).
    //
    // MOV EDX, EDI
    emitter.emitByte(0x89);
    emitter.emitByte(0xFA);

    emitLoadEcxEsp(4);

    // Cantidad cero: no ejecutar el bucle.
    emitter.emitByte(0x85);
    emitter.emitByte(0xC9); // TEST ECX, ECX

    const std::size_t writeCopySkip =
        emitJcc(0x84); // JE

    const std::size_t writeCopyLoop =
        emitter.size();

    // EAX = celda int32_t.
    emitter.emitByte(0x8B);
    emitter.emitByte(0x06); // MOV EAX, [ESI]

    // Guardar el byte bajo.
    emitter.emitByte(0x88);
    emitter.emitByte(0x07); // MOV [EDI], AL

    // Avanzar array: 4 bytes.
    emitter.emitByte(0x83);
    emitter.emitByte(0xC6);
    emitter.emitByte(0x04);

    // Avanzar scratch: 1 byte.
    emitter.emitByte(0x47); // INC EDI

    // Siguiente celda.
    emitter.emitByte(0x49); // DEC ECX

    const std::size_t writeLoopBack =
        emitJcc(0x85); // JNZ

    emitter.patchRel32(
        writeLoopBack,
        writeCopyLoop);

    const std::size_t writeCopyDone =
        emitter.size();

    emitter.patchRel32(
        writeCopySkip,
        writeCopyDone);


    // --------------------------------------------------------
    // ARMAR BLOQUE FUNCIÓN 70
    // --------------------------------------------------------

    result.writeParamsAddressPatch =
        emitMovEbxImm32Placeholder();

    // +00: subfunción WRITE = 2.
    emitMovDwordEbxDispImm32(0, 2);

    // +04: offsetLow = 0.
    emitMovDwordEbxDispImm32(4, 0);

    // +08: offsetHigh = 0.
    emitMovDwordEbxDispImm32(8, 0);

    // +12: cantidad.
    emitLoadEcxEsp(4);

    emitter.emitByte(0x89);
    emitter.emitByte(0x4B);
    emitter.emitByte(0x0C);

    // +16: dirección del scratch.
    //
    // La base viene en EDX: EDI quedó avanzado
    // por el bucle de conversión.
    //
    // MOV DWORD [EBX + 0x10], EDX
    emitter.emitByte(0x89);
    emitter.emitByte(0x53);
    emitter.emitByte(0x10);

    // +20: encoding RAW = 0.
    emitter.emitByte(0xC6);
    emitter.emitByte(0x43);
    emitter.emitByte(0x14);
    emitter.emitByte(0x00);

    // +21: puntero de ruta.
    emitLoadEaxEsp(8);

    emitter.emitByte(0x89);
    emitter.emitByte(0x43);
    emitter.emitByte(0x15);

    // --------------------------------------------------------
    // INT 0x40 — FUNCIÓN 70
    // --------------------------------------------------------

    emitter.emitMovEaxImm32(
        static_cast<std::int32_t>(
            KOLIBRI_FILE_IO_FUNCTION));

    emitter.emitInt(0x40);

    // EAX == 0 significa éxito.
    emitter.emitCmpEaxImm32(0);

    writeFailureBranches.push_back(
        emitJcc(0x85)); // JNE

    // Retorno correcto.
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

    const std::size_t writeEpilogue =
        emitter.size();

    emitCommonEpilogue();

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
    BranchList readEofBranches;

    emitCommonPrologue();

    /*
     * Distribución de pila:
     *
     * [ESP + 00] = EDX original
     * [ESP + 04] = ECX original: capacidad
     * [ESP + 08] = EAX original: ruta
     * [ESP + 12] = EDI preservado
     * [ESP + 16] = ESI preservado
     * [ESP + 20] = EBX preservado: array
     * [ESP + 24] = dirección de retorno
     */

    // Ruta no nula.
    emitLoadEaxEsp(8);
    emitter.emitCmpEaxImm32(0);

    readFailureBranches.push_back(
        emitJcc(0x84)); // JE

    // Array no nulo.
    emitLoadEaxEsp(20);
    emitter.emitCmpEaxImm32(0);

    readFailureBranches.push_back(
        emitJcc(0x84)); // JE

    // Capacidad > 0.
    emitLoadEcxEsp(4);
    emitCmpRegImm32(0xF9, 0);

    readFailureBranches.push_back(
        emitJcc(0x8E)); // JLE

    // Capacidad <= 65536.
    emitCmpRegImm32(
        0xF9,
        KOLIBRI_FILE_IO_MAX_BYTES);

    readFailureBranches.push_back(
        emitJcc(0x87)); // JA

    // --------------------------------------------------------
    // ARMAR BLOQUE FUNCIÓN 70
    // --------------------------------------------------------

    result.readParamsAddressPatch =
        emitMovEbxImm32Placeholder();

    // +00: subfunción READ = 0.
    emitMovDwordEbxDispImm32(0, 0);

    // +04: offsetLow = 0.
    emitMovDwordEbxDispImm32(4, 0);

    // +08: offsetHigh = 0.
    emitMovDwordEbxDispImm32(8, 0);

    // +12: capacidad solicitada.
    emitLoadEcxEsp(4);

    emitter.emitByte(0x89);
    emitter.emitByte(0x4B);
    emitter.emitByte(0x0C);

    // +16: dirección scratch.
    result.readScratchAddressPatch =
        emitMovEsiImm32Placeholder();

    emitter.emitByte(0x89);
    emitter.emitByte(0x73);
    emitter.emitByte(0x10);

    // +20: encoding RAW = 0.
    emitter.emitByte(0xC6);
    emitter.emitByte(0x43);
    emitter.emitByte(0x14);
    emitter.emitByte(0x00);

    // +21: puntero de ruta.
    emitLoadEaxEsp(8);

    emitter.emitByte(0x89);
    emitter.emitByte(0x43);
    emitter.emitByte(0x15);

    // --------------------------------------------------------
    // INT 0x40 — FUNCIÓN 70
    // --------------------------------------------------------

    emitter.emitMovEaxImm32(
        static_cast<std::int32_t>(
            KOLIBRI_FILE_IO_FUNCTION));

    emitter.emitInt(0x40);

    // Código 6 = EOF: éxito corto.
    //
    // EBX trae los bytes realmente leídos, así que
    // esta rama comparte el camino de validación
    // y desempaquetado con EAX == 0.
    emitter.emitCmpEaxImm32(
        static_cast<std::int32_t>(
            KOLIBRI_FILE_IO_EOF));

    readEofBranches.push_back(
        emitJcc(0x84)); // JE

    // EAX == 0 significa lectura correcta.
    emitter.emitCmpEaxImm32(0);

    readFailureBranches.push_back(
        emitJcc(0x85)); // JNE

    // --------------------------------------------------------
    // VALIDAR CANTIDAD DEVUELTA
    //
    // Punto común de EAX == 0 y EAX == 6 (EOF):
    // EBX = bytes leídos en ambos casos.
    // --------------------------------------------------------

    const std::size_t readValidateCount =
        emitter.size();

    emitLoadEcxEsp(4);

    // EBX = bytes leídos.
    // ECX = capacidad solicitada.
    emitter.emitByte(0x39);
    emitter.emitByte(0xCB); // CMP EBX, ECX

    readFailureBranches.push_back(
        emitJcc(0x87)); // JA

    // Guardar el número de bytes leídos.
    emitter.emitByte(0x89);
    emitter.emitByte(0xDA); // MOV EDX, EBX

    // Si no hay bytes, devolver cero.
    emitter.emitByte(0x85);
    emitter.emitByte(0xD2); // TEST EDX, EDX

    const std::size_t readCopySkip =
        emitJcc(0x84); // JE

    // --------------------------------------------------------
    // CONVERSIÓN SCRATCH byte[] -> ARRAY int32_t
    //
    // ESI ya contiene la base del scratch: se cargó
    // una única vez al armar el bloque y su
    // placeholder es el registrado en
    // result.readScratchAddressPatch.
    // INT 0x40 preserva ESI.
    // --------------------------------------------------------

    emitLoadEdiEsp(20);

    // Número de bytes a convertir.
    emitter.emitByte(0x89);
    emitter.emitByte(0xD1); // MOV ECX, EDX

    const std::size_t readCopyLoop =
        emitter.size();

    // EAX = zero_extend(byte [ESI]).
    emitter.emitByte(0x0F);
    emitter.emitByte(0xB6);
    emitter.emitByte(0x06);

    // [EDI] = EAX.
    emitter.emitByte(0x89);
    emitter.emitByte(0x07);

    // Siguiente byte.
    emitter.emitByte(0x46); // INC ESI

    // Siguiente celda int32_t.
    emitter.emitByte(0x83);
    emitter.emitByte(0xC7);
    emitter.emitByte(0x04);

    emitter.emitByte(0x49); // DEC ECX

    const std::size_t readLoopBack =
        emitJcc(0x85); // JNZ

    emitter.patchRel32(
        readLoopBack,
        readCopyLoop);

    const std::size_t readCopyDone =
        emitter.size();

    emitter.patchRel32(
        readCopySkip,
        readCopyDone);

    // EAX = bytes leídos.
    emitter.emitByte(0x89);
    emitter.emitByte(0xD0); // MOV EAX, EDX

    readSuccessBranches.push_back(
        emitJmp());

    // --------------------------------------------------------
    // ERROR READ
    // --------------------------------------------------------

    const std::size_t readFailure =
        emitter.size();

    emitter.emitMovEaxImm32(-1);

    const std::size_t readEpilogue =
        emitter.size();

    emitCommonEpilogue();

    // --------------------------------------------------------
    // RESOLVER SALTOS
    // --------------------------------------------------------

    for (const std::size_t patch :
         readFailureBranches)
    {
        emitter.patchRel32(
            patch,
            readFailure);
    }

    for (const std::size_t patch :
         readEofBranches)
    {
        emitter.patchRel32(
            patch,
            readValidateCount);
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

    // Avanzar el cursor vertical para el siguiente PRINT.
    cursorPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    emitter.emitAddEaxImm32(10);

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

    // Avanzar el cursor vertical.
    cursorPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    emitter.emitAddEaxImm32(10);

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

    // El RETURN posterior necesita recuperar EAX.
    emitter.emitPushEax();

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

    // Avanzar el cursor vertical para el siguiente PRINT_INT.
    cursorPatchList.push_back(
        emitter.emitMovEaxMem32Placeholder());

    emitter.emitAddEaxImm32(10);

    cursorPatchList.push_back(
        emitter.emitMovMemEax32Placeholder());

    /*
     * Restaurar el resultado original.
     */
    emitter.emitPopEax();

    return bufferPatch;
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
