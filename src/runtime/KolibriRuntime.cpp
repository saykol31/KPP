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
    // WIDTH = 400
    emitter.emitMovRegImm32(
        0xBB,
        (100u << 16) + 400u);

    // Y = 100
    // HEIGHT = 180
    emitter.emitMovRegImm32(
        0xB9,
        (100u << 16) + 180u);

    // Work area color
    emitter.emitMovRegImm32(
        0xBA,
        0x14FFFFFF);

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
// PRINT
// ============================================================

std::size_t
KolibriRuntime::emitPrint(
    X86Emitter& emitter,
    std::uint32_t y)
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
    emitter.emitMovRegImm32(
        0xBB,
        (35u << 16) + y);

    // Text color
    emitter.emitMovRegImm32(
        0xB9,
        0x80000000);

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

    return textPatch;
}


// ============================================================
// EXIT
// ============================================================

std::size_t
KolibriRuntime::emitPrintInteger(
    X86Emitter& emitter,
    std::uint32_t y)
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

    /*
     * Caso EAX == 0.
     */
    const std::size_t zeroLabel =
        emitter.size();

    emitter.emitDecEdi();
    emitter.emitMovEdiMem8Imm8('0');

    /*
     * Parcheamos los saltos.
     */
    emitter.patchRel32(
        zeroJump,
        zeroLabel);

    emitter.patchRel32(
        loopJump,
        conversionLoop);

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

    emitter.emitMovRegImm32(
        0xBB,
        (35u << 16) | y);

    emitter.emitMovRegImm32(
        0xB9,
        0x80000000u);

    emitter.emitMovRegImm32(
        0xBE,
        16);

    emitter.emitInt(0x40);

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

}
