

#pragma once

#include "IR.hpp"
#include "X86Emitter.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace kpp {

class X86Backend
{
public:
    std::vector<std::uint8_t> generate(
        const IRProgram& program);

private:


    struct TemporaryLocation
    {
        enum class Kind
        {
            Register,
            Memory
        };

        Kind kind = Kind::Register;

        // Registro físico:
        // EAX = 0
        // EBX = 3
        std::uint8_t registerCode = 0;

        // Offset dentro del área de datos cuando
        // el temporal está almacenado en memoria.
        std::size_t dataOffset = 0;
    };


    struct VariableLocation
    {
        std::size_t dataOffset;
    };

    struct VariableLoadPatch
    {
        std::string functionName;
        std::string name;
        std::size_t patchOffset;
    };

    struct VariableStorePatch
    {
        std::string functionName;
        std::string name;
        std::size_t patchOffset;
    };


    struct TemporarySpillPatch
    {
        std::string temporary;
        std::size_t patchOffset;
        std::size_t dataOffset;
    };

    struct TemporaryReloadPatch
    {
        std::string temporary;
        std::size_t patchOffset;
        std::size_t dataOffset;
    };

    struct StringConstPatch
    {
        std::size_t patchOffset;
        std::size_t stringIndex;
    };

    struct ControlFlowPatch
    {
        std::string label;
        std::size_t patchOffset;
    };

    // --------------------------------------------------------
    // FUNCTION CALL PATCH
    //
    // CALL rel32 se emite antes de conocer necesariamente
    // la dirección de la función destino.
    //
    // El rel32 se parchea cuando todas las funciones
    // ya fueron generadas.
    // --------------------------------------------------------

    struct FunctionCallPatch
    {
        std::string functionName;
        std::size_t patchOffset;
    };

    std::unordered_map<
        std::string,
        TemporaryLocation
    > temporaries;

    std::unordered_map<
        std::string,
        VariableLocation
    > variables;

    // --------------------------------------------------------
    // VARIABLES POR FUNCIÓN
    //
    // Cada función mantiene su propio contexto de variables.
    // Esto permite resolver LOAD/STORE después de generar
    // todas las funciones, incluso con llamadas hacia adelante.
    // --------------------------------------------------------

    std::unordered_map<
        std::string,
        std::unordered_map<
            std::string,
            VariableLocation
        >
    > functionVariables;

    // --------------------------------------------------------
    // FUNCIONES
    //
    // nombre de función -> posición dentro del código generado
    // --------------------------------------------------------

    std::unordered_map<
        std::string,
        std::size_t
    > functionEntries;

    std::vector<FunctionCallPatch>
        functionCallPatches;

    X86Emitter emitter;

    void generateFunction(
        const IRFunction& function);

    void resetTemporaries();

    void resetVariables();

    std::size_t allocateVariableData(
        std::vector<std::uint8_t>& data,
        std::size_t elementCount = 1);

    void declareVariable(
        const std::string& name,
        std::size_t dataOffset);

    void bindTemporaryToEax(
        const std::string& temporary);


    // --------------------------------------------------------
    // TEMPORARY ALLOCATOR
    // --------------------------------------------------------

    bool isEaxOccupied() const;

    bool isEbxOccupied() const;

    std::string temporaryInRegister(
        std::uint8_t registerCode) const;

    std::size_t allocateTemporarySpill(
        std::vector<std::uint8_t>& data);


    void spillTemporary(
        const std::string& temporary,
        std::vector<std::uint8_t>& data,
        std::vector<TemporarySpillPatch>& patches);


    void freeTemporary(
        const std::string& temporary);

    void bindTemporaryToMemory(
        const std::string& temporary,
        std::size_t dataOffset);

    std::uint8_t allocateRegisterForTemporary(
        const std::string& temporary,
        std::vector<std::uint8_t>& data,
        std::vector<TemporarySpillPatch>& patches);




    void bindTemporaryToEbx(
        const std::string& temporary);

    bool hasTemporary(
        const std::string& temporary) const;

    std::uint8_t getTemporaryRegister(
        const std::string& temporary) const;

    void reloadTemporary(
        const std::string& temporary,
        std::vector<std::uint8_t>& data,
        std::vector<TemporaryReloadPatch>& patches);
};

}
