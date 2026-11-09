

#pragma once

#include "IR.hpp"
#include "X86Emitter.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

        // Desplazamiento EBP-relative del slot del temporal.
        // Valores negativos: -4, -8, -12, ...
        std::int32_t dataOffset = 0;
    };


    // KPP_STACK_FRAME_REBANADA1
    struct VariableLocation
    {
        // Offset absoluto dentro de DATA.
        // Se utiliza para arrays y variables globales.
        std::size_t dataOffset = 0;

        // Indica que la variable vive en el marco EBP.
        bool isStackLocal = false;

        // Desplazamiento firmado respecto a EBP.
        // Ejemplos: -4, -8, -12, -16.
        std::int32_t stackOffset = 0;

        // KPP_FILE_WRITE_CAPACITY_PHASE3
        //
        // Capacidad declarada del array en celdas int32_t.
        // Cero para escalares y parámetros escalares.
        std::size_t arrayCapacity = 0;
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
        std::int32_t dataOffset;
    };

    struct TemporaryReloadPatch
    {
        std::string temporary;
        std::size_t patchOffset;
        std::int32_t dataOffset;
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
    // CICLO 2 — ALIAS DE ARRAYS POR NOMBRE
    //
    // Un único hueco en DATA por nombre de array para todo
    // el programa: el primer DECLARE del nombre en cada
    // función apunta al mismo almacenamiento, de modo que
    // las tablas se comparten entre funciones.
    //
    // sharedArrayOffsets: nombre -> desplazamiento en DATA
    // (lo crea el primer DECLARE que reserva el hueco).
    // sharedArraySizes: nombre -> tamaño compartido, el
    // mayor de los DECLARE que reservan (pre-pase).
    // functionArrayNames: nombres ya declarados en la
    // función actual; un DECLARE repetido dentro de la
    // misma función es una sombra de bloque y conserva
    // su hueco propio histórico.
    // --------------------------------------------------------

    std::unordered_map<
        std::string,
        std::size_t
    > sharedArrayOffsets;

    std::unordered_map<
        std::string,
        std::size_t
    > sharedArraySizes;

    std::unordered_set<
        std::string
    > functionArrayNames;

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

    // Estado del marco EBP de la función actualmente generada.
    bool frameActive = false;
    std::size_t frameSlots = 0;

    // Reserva un slot de cuatro bytes en el marco activo.
    // Devuelve desplazamientos negativos: -4, -8, -12...
    std::int32_t allocateFrameSlot();

    // Emisión de accesos a temporales derramados en el marco EBP.
    std::size_t emitMovEaxFramePlaceholder();
    std::size_t emitMovEbxFramePlaceholder();

    void generateFunction(
        const IRFunction& function);

    void resetTemporaries();

    void resetVariables();

    std::size_t allocateVariableData(
        std::vector<std::uint8_t>& data,
        std::size_t elementCount = 1);

    void declareVariable(
        const std::string& name,
        std::size_t dataOffset,
        std::size_t arrayCapacity = 0);

    void bindTemporaryToEax(
        const std::string& temporary);


    // --------------------------------------------------------
    // TEMPORARY ALLOCATOR
    // --------------------------------------------------------

    bool isEaxOccupied() const;

    bool isEbxOccupied() const;

    std::string temporaryInRegister(
        std::uint8_t registerCode) const;

    std::int32_t allocateTemporarySpill(
        std::vector<std::uint8_t>& data);


    void spillTemporary(
        const std::string& temporary,
        std::vector<std::uint8_t>& data,
        std::vector<TemporarySpillPatch>& patches);


    void freeTemporary(
        const std::string& temporary);

    void bindTemporaryToMemory(
        const std::string& temporary,
        std::int32_t dataOffset);

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
