#include "kpp/X86Emitter.hpp"

#include <stdexcept>

namespace kpp {

// ============================================================
// BYTE
// ============================================================

void X86Emitter::emitByte(
    std::uint8_t value)
{
    machineCode.push_back(value);
}

// ============================================================
// CMP EAX, EBX
//
// 39 D8
//
// Compara EAX contra EBX.
// No modifica los registros, solamente EFLAGS.
// ============================================================

void X86Emitter::emitCmpEaxEbx()
{
    machineCode.push_back(0x39);
    machineCode.push_back(0xD8);
}


// ============================================================
// SETE AL
//
// 0F 94 C0
//
// AL = 1 si EAX == EBX
// AL = 0 en caso contrario.
// ============================================================

void X86Emitter::emitSeteAl()
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x94);
    machineCode.push_back(0xC0);
}


// ============================================================
// SETNE AL
//
// 0F 95 C0
//
// AL = 1 si EAX != EBX
// ============================================================

void X86Emitter::emitSetneAl()
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x95);
    machineCode.push_back(0xC0);
}


// ============================================================
// SETL AL
//
// 0F 9C C0
//
// AL = 1 si EAX < EBX
// comparación signed.
// ============================================================

void X86Emitter::emitSetlAl()
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x9C);
    machineCode.push_back(0xC0);
}


// ============================================================
// SETLE AL
//
// 0F 9E C0
//
// AL = 1 si EAX <= EBX
// comparación signed.
// ============================================================

void X86Emitter::emitSetleAl()
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x9E);
    machineCode.push_back(0xC0);
}


// ============================================================
// SETG AL
//
// 0F 9F C0
//
// AL = 1 si EAX > EBX
// comparación signed.
// ============================================================

void X86Emitter::emitSetgAl()
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x9F);
    machineCode.push_back(0xC0);
}


// ============================================================
// SETGE AL
//
// 0F 9D C0
//
// AL = 1 si EAX >= EBX
// comparación signed.
// ============================================================

void X86Emitter::emitSetgeAl()
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x9D);
    machineCode.push_back(0xC0);
}


// ============================================================
// MOVZX EAX, AL
//
// 0F B6 C0
//
// Convierte AL = 0/1 en:
// EAX = 0/1
// ============================================================

void X86Emitter::emitMovzxEaxAl()
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0xB6);
    machineCode.push_back(0xC0);
}
// ============================================================
// MOV EAX, IMM32
// ============================================================

void X86Emitter::emitMovEaxImm32(
    std::int32_t value)
{
    machineCode.push_back(0xB8);

    const std::uint32_t v =
        static_cast<std::uint32_t>(value);

    machineCode.push_back(
        static_cast<std::uint8_t>(v & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 24) & 0xFF));
}


// ============================================================
// MOV [MEM], IMM32
// ============================================================

void X86Emitter::emitMovMemImm32(
    std::uint32_t address,
    std::int32_t value)
{
    // MOV dword [address], value
    // Opcode: C7 /0
    // ModR/M: 00 000 101 = 05

    machineCode.push_back(0xC7);
    machineCode.push_back(0x05);

    machineCode.push_back(
        static_cast<std::uint8_t>(address & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((address >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((address >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((address >> 24) & 0xFF));

    const std::uint32_t v =
        static_cast<std::uint32_t>(value);

    machineCode.push_back(
        static_cast<std::uint8_t>(v & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 24) & 0xFF));
}


// ============================================================
// MOV [MEM], IMM32 - PLACEHOLDER
// ============================================================
//
// C7 05 xx xx xx xx vv vv vv vv
//
// xx xx xx xx = dirección absoluta pendiente de parchear
// vv vv vv vv = valor inmediato
// ============================================================

std::size_t X86Emitter::emitMovMemImm32Placeholder(
    std::int32_t value)
{
    // MOV dword [absolute_address], immediate
    // Opcode: C7 /0
    // ModR/M: 05

    machineCode.push_back(0xC7);
    machineCode.push_back(0x05);

    const std::size_t addressPatch =
        emitImm32Placeholder();

    const std::uint32_t v =
        static_cast<std::uint32_t>(value);

    machineCode.push_back(
        static_cast<std::uint8_t>(v & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 24) & 0xFF));

    return addressPatch;
}


// ============================================================
// MOV EAX, [MEM] - PLACEHOLDER
// ============================================================
//
// A1 xx xx xx xx
//
// xx xx xx xx = dirección absoluta pendiente de parchear
//
// A1 = MOV EAX, moffs32
// ============================================================

std::size_t X86Emitter::emitMovEaxMem32Placeholder()
{
    machineCode.push_back(0xA1);

    return emitImm32Placeholder();
}

// ============================================================
// MOV EBX, [moffs32]
// 8B 1D xx xx xx xx
// ============================================================

std::size_t X86Emitter::emitMovEbxMem32Placeholder()
{
    /*
     * MOV EBX, [direccion]
     *
     * Codificación:
     *
     *     8B 1D xx xx xx xx
     *
     * donde xx xx xx xx es la dirección
     * absoluta de memoria.
     */

    machineCode.push_back(0x8B);
    machineCode.push_back(0x1D);

    return emitImm32Placeholder();
}



// ============================================================
// MOV [moffs32], EAX
// 89 05 xx xx xx xx
// ============================================================

void X86Emitter::emitMovMemEax32(
    std::uint32_t address)
{
    /*
     * MOV [direccion], EAX
     *
     * Codificación:
     *
     *     89 05 xx xx xx xx
     */

    machineCode.push_back(0x89);
    machineCode.push_back(0x05);

    machineCode.push_back(
        static_cast<std::uint8_t>(address & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((address >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((address >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((address >> 24) & 0xFF));
}


// ============================================================
// MOV [moffs32], EBX
// 89 1D xx xx xx xx
// ============================================================

void X86Emitter::emitMovMemEbx32(
    std::uint32_t address)
{
    /*
     * MOV [direccion], EBX
     *
     * Codificación:
     *
     *     89 1D xx xx xx xx
     */

    machineCode.push_back(0x89);
    machineCode.push_back(0x1D);

    machineCode.push_back(
        static_cast<std::uint8_t>(address & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((address >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((address >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((address >> 24) & 0xFF));
}


// ============================================================
// MOV [moffs32], EAX — PLACEHOLDER
// 89 05 xx xx xx xx
// ============================================================

std::size_t X86Emitter::emitMovMemEax32Placeholder()
{
    machineCode.push_back(0x89);
    machineCode.push_back(0x05);

    return emitImm32Placeholder();
}


// ============================================================
// MOV [moffs32], EBX — PLACEHOLDER
// 89 1D xx xx xx xx
// ============================================================

std::size_t X86Emitter::emitMovMemEbx32Placeholder()
{
    machineCode.push_back(0x89);
    machineCode.push_back(0x1D);

    return emitImm32Placeholder();
}


// ============================================================
// MOV REG, IMM32
// ============================================================

void X86Emitter::emitMovRegImm32(
    std::uint8_t opcode,
    std::uint32_t value)
{
    machineCode.push_back(opcode);

    machineCode.push_back(
        static_cast<std::uint8_t>(value & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

// ============================================================
// ADD EAX, EBX
//
// 01 D8
// ============================================================

void X86Emitter::emitAddEaxEbx()
{
    machineCode.push_back(0x01);
    machineCode.push_back(0xD8);
}


// ============================================================
// SUB EAX, EBX
//
// 29 D8
// ============================================================

void X86Emitter::emitSubEaxEbx()
{
    /*
     * SUB EAX, EBX
     *
     * Resultado:
     *
     *     EAX = EAX - EBX
     */

    machineCode.push_back(0x29);
    machineCode.push_back(0xD8);
}


// ============================================================
// MUL / IMUL EAX, EBX
// 0F AF C3
// ============================================================

void X86Emitter::emitMulEaxEbx()
{
    /*
     * IMUL EAX, EBX
     *
     * Resultado:
     *
     *     EAX = EAX * EBX
     */

    machineCode.push_back(0x0F);
    machineCode.push_back(0xAF);
    machineCode.push_back(0xC3);
}
void X86Emitter::emitMovEaxEbx()
{
    // MOV EAX, EBX
    machineCode.push_back(0x89);
    machineCode.push_back(0xD8);
}

void X86Emitter::emitMovEbxEax()
{
    // MOV EBX, EAX
    machineCode.push_back(0x89);
    machineCode.push_back(0xC3);
}

void X86Emitter::emitMovEbxEbp()
{
    // MOV EBX, EBP
    machineCode.push_back(0x89);
    machineCode.push_back(0xEB);
}

void X86Emitter::emitMovEdxEax()
{
    // MOV EDX, EAX
    machineCode.push_back(0x89);
    machineCode.push_back(0xC2);
}

void X86Emitter::emitMovEdxEbx()
{
    // MOV EDX, EBX
    machineCode.push_back(0x89);
    machineCode.push_back(0xDA);
}

void X86Emitter::emitAddEbpImm32(std::uint32_t value)
{
    // ADD EBP, imm32
    machineCode.push_back(0x81);
    machineCode.push_back(0xC5);

    machineCode.push_back(
        static_cast<std::uint8_t>(value & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 24) & 0xFF));
}
// ============================================================
// INTEGER CONVERSION
// ============================================================

// XOR EDX, EDX
// 31 D2
void X86Emitter::emitXorEdxEdx()
{
    machineCode.push_back(0x31);
    machineCode.push_back(0xD2);
}

// MOV ECX, IMM32
// B9 xx xx xx xx
void X86Emitter::emitMovEcxImm32(std::uint32_t value)
{
    machineCode.push_back(0xB9);

    machineCode.push_back(
        static_cast<std::uint8_t>(value & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

// DIV ECX
// F7 F1
void X86Emitter::emitDivEcx()
{
    machineCode.push_back(0xF7);
    machineCode.push_back(0xF1);
}

// ADD DL, IMM8
// 80 C2 xx
void X86Emitter::emitAddDlImm8(std::uint8_t value)
{
    machineCode.push_back(0x80);
    machineCode.push_back(0xC2);
    machineCode.push_back(value);
}

// DEC EDI
// 4F
void X86Emitter::emitDecEdi()
{
    machineCode.push_back(0x4F);
}

// MOV BYTE [EDI], DL
// 88 17
void X86Emitter::emitMovEdiMem8Dl()
{
    machineCode.push_back(0x88);
    machineCode.push_back(0x17);
}

// MOV EDI, IMM32
// BF xx xx xx xx
void X86Emitter::emitMovEdiImm32(std::uint32_t value)
{
    machineCode.push_back(0xBF);

    machineCode.push_back(
        static_cast<std::uint8_t>(value & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

// PUSH EAX
// 50
void X86Emitter::emitPushEax()
{
    machineCode.push_back(0x50);
}

// POP EAX
// 58
void X86Emitter::emitPopEax()
{
    machineCode.push_back(0x58);
}

// POP EBX
// 5B
void X86Emitter::emitPopEbx()
{
    machineCode.push_back(0x5B);
}

// MOV EDX, EDI
// 89 FA
void X86Emitter::emitMovEdxEdi()
{
    machineCode.push_back(0x89);
    machineCode.push_back(0xFA);
}

// MOV BYTE [EDI], IMM8
// C6 07 xx
void X86Emitter::emitMovEdiMem8Imm8(std::uint8_t value)
{
    machineCode.push_back(0xC6);
    machineCode.push_back(0x07);
    machineCode.push_back(value);
}

// ============================================================
// INT
// ============================================================

void X86Emitter::emitInt(
    std::uint8_t interrupt)
{
    machineCode.push_back(0xCD);
    machineCode.push_back(interrupt);
}


// ============================================================
// RET
// ============================================================

void X86Emitter::emitRet()
{
    machineCode.push_back(0xC3);
}


// ============================================================
// JMP rel32
//
// E9 xx xx xx xx
// ============================================================

void X86Emitter::emitJmpRel32(
    std::int32_t displacement)
{
    machineCode.push_back(0xE9);

    const std::uint32_t v =
        static_cast<std::uint32_t>(displacement);

    machineCode.push_back(
        static_cast<std::uint8_t>(v & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 24) & 0xFF));
}


// ============================================================
// JE rel32
//
// 0F 84 xx xx xx xx
// ============================================================

void X86Emitter::emitJeRel32(
    std::int32_t displacement)
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x84);

    const std::uint32_t v =
        static_cast<std::uint32_t>(displacement);

    machineCode.push_back(
        static_cast<std::uint8_t>(v & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 24) & 0xFF));
}


// ============================================================
// JNE rel32
// ============================================================

void X86Emitter::emitJneRel32(
    std::int32_t displacement)
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x85);

    const std::uint32_t v =
        static_cast<std::uint32_t>(displacement);

    machineCode.push_back(
        static_cast<std::uint8_t>(v & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 24) & 0xFF));
}


// ============================================================
// ============================================================
// JMP rel32 PLACEHOLDER
//
// E9 xx xx xx xx
// ============================================================

std::size_t X86Emitter::emitJmpRel32Placeholder()
{
    machineCode.push_back(0xE9);

    return emitRel32Placeholder();
}


// ============================================================
// JE rel32 PLACEHOLDER
//
// 0F 84 xx xx xx xx
// ============================================================

std::size_t X86Emitter::emitJeRel32Placeholder()
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x84);

    return emitRel32Placeholder();
}


// ============================================================
// JNE rel32 PLACEHOLDER
//
// 0F 85 xx xx xx xx
// ============================================================

std::size_t X86Emitter::emitJneRel32Placeholder()
{
    machineCode.push_back(0x0F);
    machineCode.push_back(0x85);

    return emitRel32Placeholder();
}


// ============================================================
// CMP EAX, IMM32
//
// 3D xx xx xx xx
// ============================================================

void X86Emitter::emitCmpEaxImm32(
    std::int32_t value)
{
    machineCode.push_back(0x3D);

    const std::uint32_t v =
        static_cast<std::uint32_t>(value);

    machineCode.push_back(
        static_cast<std::uint8_t>(v & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 8) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 16) & 0xFF));

    machineCode.push_back(
        static_cast<std::uint8_t>((v >> 24) & 0xFF));
}


// ============================================================
// CMP AH, IMM8
//
// 80 FC xx
// ============================================================

void X86Emitter::emitCmpAhImm8(
    std::uint8_t value)
{
    machineCode.push_back(0x80);
    machineCode.push_back(0xFC);
    machineCode.push_back(value);
}


// ============================================================
// IMM32 PLACEHOLDER
// ============================================================

std::size_t X86Emitter::emitImm32Placeholder()
{
    const std::size_t offset =
        machineCode.size();

    machineCode.push_back(0);
    machineCode.push_back(0);
    machineCode.push_back(0);
    machineCode.push_back(0);

    return offset;
}


// ============================================================
// REL32 PLACEHOLDER
// ============================================================

std::size_t X86Emitter::emitRel32Placeholder()
{
    return emitImm32Placeholder();
}


// ============================================================
// PATCH 32
// ============================================================

void X86Emitter::patch32(
    std::size_t offset,
    std::uint32_t value)
{
    if (offset + 4 > machineCode.size())
    {
        throw std::out_of_range(
            "K++ X86Emitter: patch32 fuera de rango");
    }

    machineCode[offset + 0] =
        static_cast<std::uint8_t>(
            value & 0xFF);

    machineCode[offset + 1] =
        static_cast<std::uint8_t>(
            (value >> 8) & 0xFF);

    machineCode[offset + 2] =
        static_cast<std::uint8_t>(
            (value >> 16) & 0xFF);

    machineCode[offset + 3] =
        static_cast<std::uint8_t>(
            (value >> 24) & 0xFF);
}


// ============================================================
// PATCH REL32
// ============================================================

void X86Emitter::patchRel32(
    std::size_t offset,
    std::size_t target)
{
    if (offset + 4 > machineCode.size())
    {
        throw std::out_of_range(
            "K++ X86Emitter: patchRel32 fuera de rango");
    }

    /*
     * El desplazamiento relativo se calcula desde
     * la instrucción que sigue al rel32.
     *
     * Por ejemplo:
     *
     * E9 xx xx xx xx
     *
     * el CPU calcula:
     *
     * EIP + displacement
     *
     * donde EIP ya apunta después de los 4 bytes.
     */

    const std::int64_t next =
        static_cast<std::int64_t>(offset + 4);

    const std::int64_t destination =
        static_cast<std::int64_t>(target);

    const std::int64_t displacement =
        destination - next;

    patch32(
        offset,
        static_cast<std::uint32_t>(
            static_cast<std::int32_t>(
                displacement)));
}


// ============================================================
// CODE
// ============================================================

const std::vector<std::uint8_t>&
X86Emitter::code() const
{
    return machineCode;
}


// ============================================================
// SIZE
// ============================================================

std::size_t
X86Emitter::size() const
{
    return machineCode.size();
}

}
