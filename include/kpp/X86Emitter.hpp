#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace kpp {

class X86Emitter
{
public:

    // --------------------------------------------------------
    // BYTE / RAW
    // --------------------------------------------------------

    void emitByte(
        std::uint8_t value);


    // --------------------------------------------------------
    // MOV
    // --------------------------------------------------------

    void emitMovEaxImm32(
        std::int32_t value);

    void emitMovMemImm32(
        std::uint32_t address,
        std::int32_t value);

    std::size_t emitMovMemImm32Placeholder(
        std::int32_t value);

    std::size_t emitMovEaxMem32Placeholder();

    std::size_t emitMovEbxMem32Placeholder();

    void emitMovMemEax32(
    std::uint32_t address);

    void emitMovMemEbx32(
        std::uint32_t address);

    std::size_t emitMovMemEax32Placeholder();

    std::size_t emitMovMemEbx32Placeholder();

    void emitMovRegImm32(
        std::uint8_t opcode,
        std::uint32_t value);

    // --------------------------------------------------------
    // ARITHMETIC
    // --------------------------------------------------------

    void emitAddEaxEbx();
    void emitAndEaxEbx();
    void emitOrEaxEbx();
    void emitSubEaxEbx();
    void emitMulEaxEbx();

    void emitMovEaxEbx();
    void emitMovEbxEax();
    void emitMovEbxEbp();
    void emitMovEdxEax();
    void emitMovEdxEbx();
    void emitAddEbpImm32(std::uint32_t value);


    // --------------------------------------------------------
    // INTEGER CONVERSION
    // --------------------------------------------------------

    void emitXorEdxEdx();

    void emitMovEcxImm32(
        std::uint32_t value);

    void emitDivEcx();

    void emitAddDlImm8(
        std::uint8_t value);

    void emitDecEdi();

    void emitMovEdiMem8Dl();

    void emitMovEdiImm32(
        std::uint32_t value);

    void emitPushEax();
    void emitPushEbx();
    void emitPopEax();
    void emitPopEbx();
    void emitMovEdxEdi();
    void emitMovEdiMem8Imm8(
        std::uint8_t value);

    // --------------------------------------------------------
    // INTERRUPT
    // --------------------------------------------------------

    void emitInt(
        std::uint8_t interrupt);


    // --------------------------------------------------------
    // CONTROL
    // --------------------------------------------------------

    void emitRet();

    void emitJmpRel32(
        std::int32_t displacement);

    void emitJeRel32(
        std::int32_t displacement);

    void emitJneRel32(
        std::int32_t displacement);

    std::size_t emitJmpRel32Placeholder();
    std::size_t emitJeRel32Placeholder();

    std::size_t emitJneRel32Placeholder();


    // --------------------------------------------------------
    // CMP
    // --------------------------------------------------------
    void emitCmpEaxImm32(
        std::int32_t value);

    void emitCmpEaxEbx();

    void emitSeteAl();
    void emitSetneAl();
    void emitSetlAl();
    void emitSetleAl();
    void emitSetgAl();
    void emitSetgeAl();

    void emitMovzxEaxAl();

    void emitCmpAhImm8(
        std::uint8_t value);


    // --------------------------------------------------------
    // PLACEHOLDERS
    // --------------------------------------------------------

    std::size_t emitImm32Placeholder();

    std::size_t emitRel32Placeholder();

    void patch32(
        std::size_t offset,
        std::uint32_t value);

    void patchRel32(
        std::size_t offset,
        std::size_t target);


    // --------------------------------------------------------
    // ACCESS
    // --------------------------------------------------------

    const std::vector<std::uint8_t>&
    code() const;

    std::size_t
    size() const;


private:

    std::vector<std::uint8_t>
        machineCode;
};

}
