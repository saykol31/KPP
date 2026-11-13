

#include "kpp/X86Backend.hpp"
#include "kpp/KolibriRuntime.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace kpp {
    void X86Backend::resetTemporaries()
    {
        temporaries.clear();
    }

    void X86Backend::bindTemporaryToEax(
        const std::string& temporary)
    {
        temporaries[temporary] = {
            TemporaryLocation::Kind::Register,
            0
        };
    }

    void X86Backend::bindTemporaryToEbx(
        const std::string& temporary)
    {
        temporaries[temporary] = {
            TemporaryLocation::Kind::Register,
            3
        };
    }


    // ============================================================
    // TEMPORARY ALLOCATOR
    // ============================================================

    bool X86Backend::isEaxOccupied() const
    {
        return !temporaryInRegister(0).empty();
    }


    bool X86Backend::isEbxOccupied() const
    {
        return !temporaryInRegister(3).empty();
    }


    std::string X86Backend::temporaryInRegister(
        std::uint8_t registerCode) const
    {
        for (const auto& entry : temporaries)
        {
            const auto& location =
                entry.second;

            if (location.kind ==
                TemporaryLocation::Kind::Register &&
                location.registerCode ==
                registerCode)
            {
                return entry.first;
            }
        }

        return {};
    }


    std::int32_t
    X86Backend::allocateTemporarySpill(
        std::vector<std::uint8_t>& data)
    {
        // Los spills ya no ocupan DATA.
        // Cada temporal recibe un slot en el marco EBP activo.
        (void)data;
        return allocateFrameSlot();
    }


    void X86Backend::bindTemporaryToMemory(
        const std::string& temporary,
        std::int32_t dataOffset)
    {
        temporaries[temporary] = {
            TemporaryLocation::Kind::Memory,
            0,
            dataOffset
        };
    }


    void X86Backend::freeTemporary(
        const std::string& temporary)
    {
        temporaries.erase(
            temporary);
    }


    void X86Backend::spillTemporary(
        const std::string& temporary,
        std::vector<std::uint8_t>& data,
        std::vector<TemporarySpillPatch>& patches)
    {
        const auto it =
            temporaries.find(temporary);

        if (it == temporaries.end())
        {
            throw std::runtime_error(
                "K++ X86Backend: no se puede hacer spill de temporal inexistente: " +
                temporary);
        }

        if (it->second.kind !=
            TemporaryLocation::Kind::Register)
        {
            throw std::runtime_error(
                "K++ X86Backend: temporal ya está en memoria: " +
                temporary);
        }

        const std::uint8_t registerCode =
            it->second.registerCode;

        const std::int32_t dataOffset =
            allocateTemporarySpill(data);

        std::size_t patchOffset = 0;

        if (registerCode == 0)
        {
            /*
            * MOV [direccion], EAX
            */
            // MOV [EBP + displacement], EAX
            patchOffset = emitter.size() + 2;
            emitter.emitMovEbpRelativeEax(0);
        }
        else if (registerCode == 3)
        {
            /*
            * MOV [direccion], EBX
            */
            // MOV [EBP + displacement], EBX
            patchOffset = emitter.size() + 2;
            emitter.emitMovEbpRelativeEbx(0);
        }
        else
        {
            throw std::runtime_error(
                "K++ X86Backend: registro no soportado durante spill.");
        }

        patches.push_back({
            temporary,
            patchOffset,
            dataOffset
        });

        bindTemporaryToMemory(
            temporary,
            dataOffset);
    }


    std::uint8_t
    X86Backend::allocateRegisterForTemporary(
        const std::string& temporary,
        std::vector<std::uint8_t>& data,
        std::vector<TemporarySpillPatch>& patches)
    {
        if (!temporary.empty() &&
            hasTemporary(temporary))
        {
            const auto& location =
                temporaries.at(temporary);

            if (location.kind ==
                TemporaryLocation::Kind::Register)
            {
                return location.registerCode;
            }
        }

        /*
        * Primero intentamos EAX.
        */
        if (!isEaxOccupied())
        {
            bindTemporaryToEax(
                temporary);

            return 0;
        }

        /*
        * Después intentamos EBX.
        */
        if (!isEbxOccupied())
        {
            bindTemporaryToEbx(
                temporary);

            return 3;
        }

        /*
        * Ambos registros están ocupados.
        *
        * Seleccionamos temporalmente EAX
        * para enviarlo a memoria.
        *
        * Más adelante podremos mejorar
        * esta política mediante:
        *
        *     - liveness
        *     - uso futuro
        *     - register pressure
        *     - mejor selección de spill
        */

        const std::string eaxTemporary =
            temporaryInRegister(0);

        if (!eaxTemporary.empty())
        {
            spillTemporary(
                eaxTemporary,
                data,
                patches);

            bindTemporaryToEax(
                temporary);

            return 0;
        }

        const std::string ebxTemporary =
            temporaryInRegister(3);

        if (!ebxTemporary.empty())
        {
            spillTemporary(
                ebxTemporary,
                data,
                patches);

            bindTemporaryToEbx(
                temporary);

            return 3;
        }

        throw std::runtime_error(
            "K++ X86Backend: no hay registro disponible para temporal.");
    }


    // ============================================================
    // TEMPORARY REGISTER ACCESS
    // ============================================================

    std::uint8_t X86Backend::getTemporaryRegister(
        const std::string& temporary) const
    {
        const auto it =
            temporaries.find(temporary);

        if (it == temporaries.end())
        {
            throw std::runtime_error(
                "K++ X86: temporal no encontrado: " +
                temporary);
        }

        if (it->second.kind !=
            TemporaryLocation::Kind::Register)
        {
            throw std::runtime_error(
                "K++ X86: temporal está almacenado en memoria: " +
                temporary);
        }

        return it->second.registerCode;
    }

    void X86Backend::reloadTemporary(
        const std::string& temporary,
        std::vector<std::uint8_t>& data,
        std::vector<TemporaryReloadPatch>& patches)
    {
        (void)data;
        const auto it =
            temporaries.find(temporary);

        if (it == temporaries.end())
        {
            throw std::runtime_error(
                "K++ X86Backend: no se puede recargar temporal inexistente: " +
                temporary);
        }

        if (it->second.kind !=
            TemporaryLocation::Kind::Memory)
        {
            throw std::runtime_error(
                "K++ X86Backend: temporal no está en memoria: " +
                temporary);
        }

        const std::int32_t dataOffset =
            it->second.dataOffset;

        /*
         * Generamos:
         *
         *     MOV EAX, [EBP + displacement]
         *
         * El displacement negativo se parcheará al final
         * de generate(), sin utilizar dataBase.
         */

        const std::size_t patchOffset =
            emitMovEaxFramePlaceholder();

        patches.push_back({
            temporary,
            patchOffset,
            dataOffset
        });

        /*
         * El temporal acaba de ser recargado
         * en EAX.
         */

        bindTemporaryToEax(
            temporary);
    }


    void X86Backend::resetVariables()
    {
        variables.clear();

        // Los escalares globales (DATA) estan visibles en
        // todas las funciones: se siembran tras cada limpieza.
        for (const auto& global : globalVariables)
        {
            variables[global.first] = global.second;
        }

        // CICLO 2: los nombres de array declarados son
        // estado por función; los huecos compartidos
        // (sharedArrayOffsets) viven para todo el programa.
        functionArrayNames.clear();
    }

    std::size_t
    X86Backend::allocateVariableData(
        std::vector<std::uint8_t>& data,
        std::size_t elementCount)
    {
        const std::size_t offset =
            data.size();

        data.resize(
            data.size() + elementCount * 4,
            0);

        return offset;
    }

    void X86Backend::declareVariable(
        const std::string& name,
        std::size_t dataOffset,
        std::size_t arrayCapacity)
    {
        variables[name] = {
            dataOffset,
            false,
            0,
            arrayCapacity
        };
    }

    bool X86Backend::hasTemporary(
        const std::string& temporary) const
    {
        return temporaries.find(temporary)
            != temporaries.end();
    }




// ============================================================
// K++ REBANADA 2 — MARCOS Y TEMPORALES EBP-RELATIVE
// ============================================================

std::int32_t X86Backend::allocateFrameSlot()
{
    if (!frameActive)
    {
        throw std::runtime_error(
            "K++ X86Backend: intento de reservar un slot "
            "sin un marco EBP activo.");
    }

    if (frameSlots >= 0x1FFFFFFF)
    {
        throw std::runtime_error(
            "K++ X86Backend: marco de pila demasiado grande.");
    }

    ++frameSlots;

    return -static_cast<std::int32_t>(frameSlots * 4);
}


std::size_t X86Backend::emitMovEaxFramePlaceholder()
{
    if (!frameActive)
    {
        throw std::runtime_error(
            "K++ X86Backend: reload EAX fuera de un marco activo.");
    }

    // 8B 85 disp32
    const std::size_t patchOffset = emitter.size() + 2;
    emitter.emitMovEaxEbpRelative(0);
    return patchOffset;
}


std::size_t X86Backend::emitMovEbxFramePlaceholder()
{
    if (!frameActive)
    {
        throw std::runtime_error(
            "K++ X86Backend: reload EBX fuera de un marco activo.");
    }

    // 8B 9D disp32
    const std::size_t patchOffset = emitter.size() + 2;
    emitter.emitMovEbxEbpRelative(0);
    return patchOffset;
}


// ============================================================
// CONSTANTES
// ============================================================

static constexpr std::uint32_t
KOLIBRI_HEADER_SIZE = 36;


// ============================================================
// GENERATE
// ============================================================

std::vector<std::uint8_t>
X86Backend::generate(
    const IRProgram& program)
{
    /*
     * ========================================================
     * STRING POOL
     * ========================================================
     *
     * strings[0] = título
     * strings[1...] = PRINT
     */

    std::vector<std::string> strings;

    std::vector<std::string> stringConstTexts;

    std::vector<std::uint8_t> data;

    // KPP_FILE_IO_RUNTIME_BACKEND_PHASE2
    //
    // No reservar 64 KiB ni emitir las rutinas File I/O
    // cuando el programa no utiliza estas instrucciones.

    bool needsFileIoRuntime = false;

    for (const auto& function : program.functions)
    {
        for (const auto& instruction : function.instructions)
        {
            if (instruction.opcode == IROpcode::FileRead ||
                instruction.opcode == IROpcode::FileWrite)
            {
                needsFileIoRuntime = true;
                break;
            }
        }

        if (needsFileIoRuntime)
        {
            break;
        }
    }

    std::size_t integerBufferOffset = 0;

    // KPP_CURSOR_DATA
    std::size_t cursorDataOffset = 0;

    // Buffer de líneas de la Konsole (DATA): cabeza
    // circular, contador, primera visible, cursor de
    // repintado y las 512 ranuras de 64 bytes.
    std::size_t lineHeadDataOffset = 0;
    std::size_t lineCountDataOffset = 0;
    std::size_t firstVisibleDataOffset = 0;
    std::size_t repaintCursorDataOffset = 0;
    std::size_t lineBufferDataOffset = 0;
    std::size_t thumbTopDataOffset = 0;
    std::size_t thumbHeightDataOffset = 0;
    std::size_t dragStateDataOffset = 0;
    std::size_t dragGrabDataOffset = 0;

    // --------------------------------------------------------
    // Título automático
    // --------------------------------------------------------

    strings.push_back(
        "K++ - KolibriOS");



    // --------------------------------------------------------
    // Buscar PRINT
    // --------------------------------------------------------

    for (const auto& function :
         program.functions)
    {
        for (const auto& instruction :
             function.instructions)
        {


            if (instruction.opcode ==
                IROpcode::StringConst)
            {
                stringConstTexts.push_back(
                    instruction.name);
            }
        }
    }


    // --------------------------------------------------------
    // Cantidad de PRINT
    // --------------------------------------------------------



    // ========================================================
    // RUNTIME
    // ========================================================

    KolibriRuntime runtime;

    const KolibriRuntimeResult runtimeResult =
        runtime.emitGraphicsRuntime(
            emitter);

    // Runtime File I/O: solo cuando el IR lo necesita.
    KolibriFileIoRuntimeResult fileIoRuntimeResult{};

    if (needsFileIoRuntime)
    {
        fileIoRuntimeResult =
            runtime.emitFileIoRuntime(emitter);
    }


    // ========================================================
    // MAIN BODY
    // ========================================================
    //
    // Aquí comienza realmente el código generado
    // correspondiente al programa C++.
    //
    // START
    //   |
    //   +--> DRAW_WINDOW
    //   |
    //   +--> MAIN_BODY
    //
    // MAIN_BODY
    //   |
    //   +--> PRINT
    //   +--> PRINT
    //   +--> RETURN
    //   |
    //   +--> EVENT_LOOP
    //

    // --------------------------------------------------------
    // MAIN BODY
    //
    // La dirección real de main se conoce cuando encontramos
    // FUNCTION_BEGIN main.
    // --------------------------------------------------------


    std::size_t mainBody = 0;




    std::vector<std::size_t>
        integerBufferPatches;

    std::vector<VariableLoadPatch>
        variableLoadPatches;


    std::vector<VariableStorePatch>
        variableStorePatches;


    std::vector<TemporarySpillPatch>
        temporarySpillPatches;

    std::vector<TemporaryReloadPatch>
        temporaryReloadPatches;

    // KPP_FILE_WRITE_HANDLER_PHASE3
    //
    // Registra las direcciones que resolverá la fase 5.
    // En esta fase no se calculan direcciones absolutas.

    struct FileWritePatch
    {
        std::string functionName;
        std::string arrayName;

        std::size_t pathDataOffset = 0;

        std::size_t pathAddressPatch = 0;
        std::size_t arrayAddressPatch = 0;

        std::size_t capacity = 0;
    };

    std::vector<FileWritePatch>
        fileWritePatches;

    // KPP_FILE_READ_PATCH_PHASE4
    //
    // Registra las direcciones que resolverá la fase 5.
    // FILE_READ y FILE_WRITE se resolverán conjuntamente.
    //
    // En esta fase no se calculan direcciones absolutas.

    struct FileReadPatch
    {
        std::string functionName;
        std::string arrayName;

        std::size_t pathDataOffset = 0;

        std::size_t pathAddressPatch = 0;
        std::size_t arrayAddressPatch = 0;

        std::size_t capacity = 0;
    };

    std::vector<FileReadPatch>
        fileReadPatches;


    std::vector<StringConstPatch>
        stringConstPatches;

    std::vector<ControlFlowPatch>
        controlFlowPatches;

    std::unordered_map<
        std::string,
        std::size_t
    > labels;

    // ========================================================
    // RETURN PATCHES
    //
    // Puede haber múltiples RETURN debido a if / else.
    // Cada RETURN genera su propio JMP hacia el event loop.
    // ========================================================

    std::vector<std::size_t>
        returnEventLoopPatches;


    std::size_t
        printIndex = 0;

    std::size_t
        stringConstIndex = 0;

    functionEntries.clear();
    functionCallPatches.clear();
    functionVariables.clear();

    resetTemporaries();
    resetVariables();


    // ========================================================
    // CICLO 2 — PRE-PASE DE ARRAYS COMPARTIDOS
    //
    // Tamaño compartido de cada nombre de array: el mayor
    // de sus DECLARE que reservan hueco (el primero de
    // cada función; las sombras de bloque no cuentan).
    // ========================================================

    sharedArrayOffsets.clear();
    sharedArraySizes.clear();

    for (const auto& function : program.functions)
    {
        std::unordered_set<std::string> declaredHere;

        for (const auto& instruction : function.instructions)
        {
            if (instruction.opcode == IROpcode::Declare &&
                instruction.value > 0)
            {
                const std::string sourceName =
                    instruction.name.substr(
                        0,
                        instruction.name.find('@'));

                if (declaredHere.insert(sourceName).second)
                {
                    const std::size_t size =
                        static_cast<std::size_t>(
                            instruction.value);

                    const auto it =
                        sharedArraySizes.find(sourceName);

                    if (it == sharedArraySizes.end() ||
                        size > it->second)
                    {
                        sharedArraySizes[sourceName] = size;
                    }
                }
            }
        }
    }

    // ========================================================
    // VARIABLES GLOBALES — ESCALARES COMPARTIDOS EN DATA
    //
    // Cada Declare global reserva una celda en DATA y queda
    // registrado para todo el programa; resetVariables() lo
    // siembra en el mapa de trabajo de cada funcion.
    // ========================================================

    globalVariables.clear();

    for (const auto& instruction : program.globals)
    {
        if (instruction.opcode == IROpcode::Declare)
        {
            const std::size_t offset =
                allocateVariableData(data);

            globalVariables[instruction.name] = {
                offset,
                false,
                0,
                0
            };
        }
    }

    // ========================================================
    // FUNCIÓN ACTUAL
    // ========================================================

    std::string currentFunctionName;

    // REBANADA 2:
    // frameActive y frameSlots son miembros de X86Backend.
    // allocateFrameSlot() es compartido por variables locales
    // y temporales derramados.
    std::size_t frameSizePatch = 0;

    frameActive = false;
    frameSlots = 0;

    // Finaliza el marco de la función actual.
    //
    // Las funciones normales retornan mediante RET.
    // main restaura EBP/ESP y salta al event loop de KolibriOS.
    const auto finishCurrentFunctionFrame = [&]()
    {
        if (!frameActive)
        {
            return;
        }

        // Retorno implícito: valor cero.
        emitter.emitMovEaxImm32(0);

        emitter.emitMovEspEbp();
        emitter.emitPopEbp();

        if (currentFunctionName == "main")
        {
            emitter.emitByte(0xE9);

            returnEventLoopPatches.push_back(
                emitter.emitRel32Placeholder());
        }
        else
        {
            emitter.emitRet();
        }

        const std::size_t frameBytes =
            frameSlots * 4;

        emitter.patch32(
            frameSizePatch,
            static_cast<std::uint32_t>(frameBytes));

        frameActive = false;
    };

    // ========================================================
    // LABELS LOCALES POR FUNCIÓN
    //
    // El IR puede reutilizar L0, L1, L2 en distintas funciones.
    // El backend necesita nombres físicamente únicos.
    //
    // IR:
    //     L0
    //
    // Backend:
    //     funcion::L0
    // ========================================================

    const auto makeScopedLabel =
        [&currentFunctionName](const std::string& label)
        {
            if (currentFunctionName.empty())
            {
                throw std::runtime_error(
                    "K++ X86Backend: etiqueta fuera de una función: " +
                    label);
            }

            return currentFunctionName + "::" + label;
        };

    // ========================================================
    // GENERAR INSTRUCCIONES IR
    // ========================================================

    for (const auto& function :
         program.functions)
    {
        for (const auto& instruction :
             function.instructions)
        {

            // ------------------------------------------------
            // FUNCTION BEGIN
            // ------------------------------------------------
            //
            // FUNCTION_BEGIN suma
            //
            // Registra la dirección de entrada de la función.
            // ------------------------------------------------

            if (instruction.opcode ==
                IROpcode::FunctionBegin)
            {
                finishCurrentFunctionFrame();
                if (instruction.name.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FUNCTION_BEGIN sin nombre");
                }

                if (functionEntries.find(
                        instruction.name) !=
                    functionEntries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: función duplicada: " +
                        instruction.name);
                }

                functionEntries[
                    instruction.name] =
                    emitter.size();

                // ------------------------------------------------
                // Guardar el contexto de variables de la función
                // anterior antes de comenzar la siguiente.
                // ------------------------------------------------

                if (!currentFunctionName.empty())
                {
                    functionVariables[
                        currentFunctionName] =
                        variables;
                }

                currentFunctionName =
                    instruction.name;

                // ------------------------------------------------
                // MAIN
                //
                // El runtime debe entrar exactamente aquí.
                // ------------------------------------------------

                // Todas las funciones, incluido main, tienen marco.
                // mainBody apunta al comienzo exacto de su prólogo.
                if (instruction.name == "main")
                {
                    mainBody = emitter.size();
                }

                emitter.emitPushEbp();
                emitter.emitMovEbpEsp();

                frameSlots = 0;

                frameSizePatch =
                    emitter.emitSubEspImm32Placeholder();

                frameActive = true;

                // ------------------------------------------------
                // Cada función tiene su propio estado local.
                // ------------------------------------------------

                resetTemporaries();
                resetVariables();

                // ------------------------------------------------
                // PARÁMETROS DE FUNCIÓN
                //
                // Convención inicial de K++:
                //
                // parámetro 0 -> EAX
                // parámetro 1 -> EBX
                //
                // Los parámetros se materializan inmediatamente
                // como variables backend normales.
                //
                // Esto evita crear alias peligrosos dentro del
                // allocator:
                //
                //     a  -> EAX
                //     %0 -> EAX
                //
                // En su lugar:
                //
                //     EAX -> [a]
                //     EBX -> [b]
                //
                // Después, LOAD a / LOAD b utilizan el camino
                // normal de variables y temporales.
                //
                // FUNCTION suma a, b
                // ------------------------------------------------

                if (!instruction.operand.empty())
                {
                    const std::string parameters =
                        instruction.operand;

                    std::size_t start = 0;
                    std::size_t parameterIndex = 0;

                    while (start < parameters.size())
                    {
                        const std::size_t comma =
                            parameters.find(',', start);

                        const std::size_t end =
                            (comma == std::string::npos)
                                ? parameters.size()
                                : comma;

                        std::string parameter =
                            parameters.substr(
                                start,
                                end - start);

                        while (!parameter.empty() &&
                               parameter.front() == ' ')
                        {
                            parameter.erase(
                                parameter.begin());
                        }

                        while (!parameter.empty() &&
                               parameter.back() == ' ')
                        {
                            parameter.pop_back();
                        }

                        if (parameter.empty())
                        {
                            throw std::runtime_error(
                                "K++ X86Backend: parámetro vacío en FUNCTION " +
                                instruction.name);
                        }

                        if (frameActive)
                        {
                            const std::int32_t stackOffset =
                                allocateFrameSlot();

                            variables[parameter] = {
                                0,
                                true,
                                stackOffset
                            };

                            if (parameterIndex == 0)
                            {
                                emitter.emitMovEbpRelativeEax(
                                    stackOffset);
                            }
                            else if (parameterIndex == 1)
                            {
                                emitter.emitMovEbpRelativeEbx(
                                    stackOffset);
                            }
                            else
                            {
                                // CICLO 4: el argumento llega
                                // por la pila del llamador:
                                // [EBP+8] es el argumento 2,
                                // [EBP+12] el 3, etc. EAX ya
                                // entregó el argumento 0 y EBX
                                // el 1 (índices anteriores), así
                                // que EAX sirve de lanzadera.
                                emitter.emitMovEaxEbpRelative(
                                    8 + static_cast<std::int32_t>(
                                            (parameterIndex - 2)
                                            * 4));

                                emitter.emitMovEbpRelativeEax(
                                    stackOffset);
                            }
                        }
                        else
                        {
                            // main conserva su mecanismo anterior
                            // (sin llamador que prepare la pila:
                            // máximo dos parámetros).
                            if (parameterIndex > 1)
                            {
                                throw std::runtime_error(
                                    "K++ X86Backend: más de 2 parámetros "
                                    "no soportados todavía en función " +
                                    instruction.name);
                            }

                            const std::size_t dataOffset =
                                allocateVariableData(data);

                            declareVariable(
                                parameter,
                                dataOffset);

                            std::size_t patchOffset = 0;

                            if (parameterIndex == 0)
                            {
                                patchOffset =
                                    emitter.emitMovMemEax32Placeholder();
                            }
                            else
                            {
                                patchOffset =
                                    emitter.emitMovMemEbx32Placeholder();
                            }

                            variableStorePatches.push_back({
                                currentFunctionName,
                                parameter,
                                patchOffset
                            });
                        }

                        ++parameterIndex;

                        if (comma == std::string::npos)
                        {
                            break;
                        }

                        start = comma + 1;
                    }
                }
            }


            // ------------------------------------------------
            // CONST
            // ------------------------------------------------


            if (instruction.opcode ==
                IROpcode::Const)
            {
                /*
                * CONST valor -> temporal
                *
                * Dejamos que el allocator decida
                * dónde colocar el temporal:
                *
                *     EAX
                *     EBX
                *     o memoria mediante spill.
                */

                const std::uint8_t registerCode =
                    allocateRegisterForTemporary(
                        instruction.result,
                        data,
                        temporarySpillPatches);

                    std::cerr
                        << "[ALLOC] "
                        << instruction.result
                        << " -> ";

                    if (hasTemporary(instruction.result))
                    {
                        const auto& location =
                            temporaries.at(instruction.result);

                        if (location.kind ==
                            TemporaryLocation::Kind::Register)
                        {
                            if (location.registerCode == 0)
                                std::cerr << "EAX";
                            else if (location.registerCode == 3)
                                std::cerr << "EBX";
                            else
                                std::cerr << "REG("
                                        << static_cast<int>(
                                                location.registerCode)
                                        << ")";
                        }
                        else
                        {
                            std::cerr
                                << "MEM["
                                << location.dataOffset
                                << "]";
                        }
                    }

                    std::cerr << '\n';


                if (registerCode == 0)
                {
                    /*
                    * EAX
                    *
                    * MOV EAX, inmediato
                    */

                    emitter.emitMovEaxImm32(
                        instruction.value);
                }
                else if (registerCode == 3)
                {
                    /*
                    * EBX
                    *
                    * MOV EBX, inmediato
                    */

                    emitter.emitMovRegImm32(
                        0xBB,
                        static_cast<std::uint32_t>(
                            instruction.value));
                }
                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: registro no soportado para CONST.");
                }
            }



            // ------------------------------------------------
            // STRING_CONST
            // ------------------------------------------------

            else if (instruction.opcode ==
                IROpcode::StringConst)
            {
                /*
                 * STRING_CONST "texto" -> temporal
                 *
                 * El valor real del temporal es la dirección
                 * absoluta del texto dentro de DATA.
                 *
                 * En este punto DATA todavía no conoce su
                 * dirección absoluta, por lo que emitimos:
                 *
                 *     MOV EAX, 0
                 *     o
                 *     MOV EBX, 0
                 *
                 * y guardamos un patch para resolverlo después.
                 */

                const std::uint8_t registerCode =
                    allocateRegisterForTemporary(
                        instruction.result,
                        data,
                        temporarySpillPatches);

                std::size_t patchOffset = 0;

                if (registerCode == 0)
                {
                    /*
                     * EAX
                     *
                     * El inmediato comienza un byte después
                     * del opcode B8.
                     */
                    patchOffset =
                        emitter.size() + 1;

                    emitter.emitMovEaxImm32(0);
                }
                else if (registerCode == 3)
                {
                    /*
                     * EBX
                     *
                     * El inmediato comienza un byte después
                     * del opcode BB.
                     */
                    patchOffset =
                        emitter.size() + 1;

                    emitter.emitMovRegImm32(
                        0xBB,
                        0);
                }
                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: registro no soportado para STRING_CONST.");
                }

                stringConstPatches.push_back({
                    patchOffset,
                    stringConstIndex
                });

                ++stringConstIndex;
            }


            // ------------------------------------------------
            // DECLARE
            // ------------------------------------------------

            else if (instruction.opcode ==
                IROpcode::Declare)
            {
                if (frameActive && instruction.value <= 0)
                {
                    // Variable escalar local.
                    const std::int32_t stackOffset =
                        allocateFrameSlot();

                    variables[instruction.name] = {
                        0,
                        true,
                        stackOffset
                    };
                }
                else if (instruction.value > 0)
                {
                    // ------------------------------------------------
                    // CICLO 2 — ARRAY EN DATA, CON ALIAS POR NOMBRE
                    //
                    // El primer DECLARE de un nombre en cada
                    // función comparte un único hueco en DATA:
                    // las funciones que declaran el mismo array
                    // trabajan sobre el mismo almacenamiento.
                    // Un DECLARE repetido dentro de la misma
                    // función es una sombra de bloque y conserva
                    // su hueco propio histórico.
                    // ------------------------------------------------

                    const std::string sourceName =
                        instruction.name.substr(
                            0,
                            instruction.name.find('@'));

                    const std::size_t declaredSize =
                        static_cast<std::size_t>(
                            instruction.value);

                    if (!functionArrayNames
                             .insert(sourceName)
                             .second)
                    {
                        // Sombra dentro de la misma función.
                        const std::size_t offset =
                            allocateVariableData(
                                data,
                                declaredSize);

                        declareVariable(
                            instruction.name,
                            offset,
                            declaredSize);
                    }
                    else
                    {
                        const auto sharedIt =
                            sharedArrayOffsets.find(
                                sourceName);

                        if (sharedIt !=
                            sharedArrayOffsets.end())
                        {
                            // Alias: mismo hueco, sin reservar.
                            declareVariable(
                                instruction.name,
                                sharedIt->second,
                                declaredSize);
                        }
                        else
                        {
                            // Primera reserva del nombre: se
                            // reserva el tamaño compartido
                            // (el mayor declarado).
                            const std::size_t offset =
                                allocateVariableData(
                                    data,
                                    sharedArraySizes[
                                        sourceName]);

                            sharedArrayOffsets[
                                sourceName] = offset;

                            declareVariable(
                                instruction.name,
                                offset,
                                declaredSize);
                        }
                    }
                }
                else
                {
                    // Variables de main permanecen en DATA.
                    const std::size_t offset =
                        allocateVariableData(
                            data,
                            1);

                    declareVariable(
                        instruction.name,
                        offset,
                        0);
                }
            }

            // ------------------------------------------------
            // STORE
            // ------------------------------------------------

            else if (instruction.opcode ==
                     IROpcode::Store)
            {
                /*
                 * STORE x <- %N
                 *
                 * El valor a almacenar está en un temporal.
                 *
                 * Si el temporal está en EAX:
                 *
                 *     MOV [direccion_de_x], EAX
                 *
                 * Si el temporal está en EBX:
                 *
                 *     MOV [direccion_de_x], EBX
                 *
                 * La dirección absoluta se resolverá después,
                 * cuando conozcamos dataBase.
                 */

                const auto variableIt =
                    variables.find(instruction.name);

                if (variableIt == variables.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: STORE de variable no declarada: " +
                        instruction.name);
                }

                if (instruction.operand.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: STORE sin temporal: " +
                        instruction.name);
                }

                const std::uint8_t reg =
                    getTemporaryRegister(
                        instruction.operand);

                if (variableIt->second.isStackLocal)
                {
                    // STORE sobre variable local del marco.
                    if (reg == 0)
                    {
                        emitter.emitMovEbpRelativeEax(
                            variableIt->second.stackOffset);
                    }
                    else if (reg == 3)
                    {
                        emitter.emitMovEbpRelativeEbx(
                            variableIt->second.stackOffset);
                    }
                    else
                    {
                        throw std::runtime_error(
                            "K++ X86Backend: STORE usa registro no soportado.");
                    }
                }
                else
                {
                    // STORE sobre variable DATA.
                    std::size_t patchOffset = 0;

                    if (reg == 0)
                    {
                        patchOffset =
                            emitter.emitMovMemEax32Placeholder();
                    }
                    else if (reg == 3)
                    {
                        patchOffset =
                            emitter.emitMovMemEbx32Placeholder();
                    }
                    else
                    {
                        throw std::runtime_error(
                            "K++ X86Backend: STORE usa registro no soportado.");
                    }

                    variableStorePatches.push_back({
                        currentFunctionName,
                        instruction.name,
                        patchOffset
                    });
                }

                /*
                 * STORE consume el temporal.
                 *
                 * Después de copiar el valor a la variable,
                 * el temporal ya no necesita permanecer ocupado
                 * en EAX/EBX.
                 */
                freeTemporary(
                    instruction.operand);
            }

            // ------------------------------------------------
            // ARRAY STORE
            // ------------------------------------------------
            else if (instruction.opcode == IROpcode::ArrayStore)
            {
                if (variables.find(instruction.name) == variables.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: ARRAY_STORE de array no declarado: " +
                        instruction.name);
                }

                if (!hasTemporary(instruction.operand) ||
                    !hasTemporary(instruction.result))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: temporal inexistente en ARRAY_STORE.");
                }

                const auto indexLoc = temporaries.at(instruction.operand);
                const auto valueLoc = temporaries.at(instruction.result);

                // Preservar ambos registros mientras calculamos la dirección.
                emitter.emitPushEax();
                emitter.emitPushEbx();

                if (indexLoc.kind == TemporaryLocation::Kind::Memory)
                {
                    const auto patch = emitMovEaxFramePlaceholder();
                    temporaryReloadPatches.push_back({
                        instruction.operand, patch, indexLoc.dataOffset
                    });
                }
                else if (indexLoc.registerCode == 3)
                {
                    emitter.emitMovEaxEbx();
                }
                else if (indexLoc.registerCode != 0)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: registro de índice no soportado.");
                }

                const auto addressPatch =
                    emitter.emitLeaEdxEaxScaled4Placeholder();

                variableLoadPatches.push_back({
                    currentFunctionName, instruction.name, addressPatch
                });

                // La dirección queda en EDX; restaurar los registros originales.
                emitter.emitPopEbx();
                emitter.emitPopEax();

                if (valueLoc.kind == TemporaryLocation::Kind::Memory)
                {
                    const auto patch = emitMovEaxFramePlaceholder();
                    temporaryReloadPatches.push_back({
                        instruction.result, patch, valueLoc.dataOffset
                    });
                    emitter.emitMovMemEdxEax32();
                }
                else if (valueLoc.registerCode == 0)
                {
                    emitter.emitMovMemEdxEax32();
                }
                else if (valueLoc.registerCode == 3)
                {
                    emitter.emitMovMemEdxEbx32();
                }
                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: registro de valor no soportado.");
                }

                freeTemporary(instruction.operand);
                freeTemporary(instruction.result);
            }

            // ------------------------------------------------
            // ARRAY LOAD
            // ------------------------------------------------
            else if (instruction.opcode == IROpcode::ArrayLoad)
            {
                if (variables.find(instruction.name) == variables.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: ARRAY_LOAD de array no declarado: " +
                        instruction.name);
                }

                if (!hasTemporary(instruction.operand))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: temporal de índice inexistente en ARRAY_LOAD.");
                }

                const auto indexLoc = temporaries.at(instruction.operand);

                emitter.emitPushEax();
                emitter.emitPushEbx();

                if (indexLoc.kind == TemporaryLocation::Kind::Memory)
                {
                    const auto patch = emitMovEaxFramePlaceholder();
                    temporaryReloadPatches.push_back({
                        instruction.operand, patch, indexLoc.dataOffset
                    });
                }
                else if (indexLoc.registerCode == 3)
                {
                    emitter.emitMovEaxEbx();
                }
                else if (indexLoc.registerCode != 0)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: registro de índice no soportado.");
                }

                const auto addressPatch =
                    emitter.emitLeaEdxEaxScaled4Placeholder();

                variableLoadPatches.push_back({
                    currentFunctionName, instruction.name, addressPatch
                });

                emitter.emitPopEbx();
                emitter.emitPopEax();

                // El asignador puede hacer spill; EDX conserva la dirección.
                const auto resultReg = allocateRegisterForTemporary(
                    instruction.result, data, temporarySpillPatches);

                if (resultReg == 0)
                {
                    emitter.emitMovEaxEdxMem32();
                }
                else if (resultReg == 3)
                {
                    emitter.emitMovEbxEdxMem32();
                }
                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: registro de resultado no soportado.");
                }

                freeTemporary(instruction.operand);
            }
            // LOAD
            // ------------------------------------------------


            else if (instruction.opcode ==
                     IROpcode::Load)
            {
                /*
                 * LOAD x -> %N
                 *
                 * El resultado de LOAD utiliza el mismo
                 * allocator que CONST, ADD y MUL.
                 *
                 * El sistema anterior utilizaba:
                 *
                 *     temporaries.size() % 2
                 *
                 * Eso ya no es válido porque los temporales
                 * pueden estar:
                 *
                 *     EAX
                 *     EBX
                 *     MEMORIA
                 *
                 * y el allocator puede realizar spills.
                 *
                 * Por tanto LOAD delega completamente la
                 * selección del registro a:
                 *
                 *     allocateRegisterForTemporary()
                 */

                const auto variableIt =
                    variables.find(
                        instruction.name);

                if (variableIt ==
                    variables.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: LOAD de variable no declarada: " +
                        instruction.name);
                }

                /*
                 * Pedimos al allocator un registro para
                 * el temporal resultado.
                 *
                 * Puede devolver:
                 *
                 *     0 -> EAX
                 *     3 -> EBX
                 *
                 * Si ambos están ocupados, el allocator
                 * realizará el spill correspondiente.
                 */

                const std::uint8_t registerCode =
                    allocateRegisterForTemporary(
                        instruction.result,
                        data,
                        temporarySpillPatches);

                /*
                 * Generamos el LOAD dependiendo del registro
                 * realmente asignado.
                 */

                if (variableIt->second.isStackLocal)
                {
                    // LOAD desde el marco de la invocación actual.
                    if (registerCode == 0)
                    {
                        emitter.emitMovEaxEbpRelative(
                            variableIt->second.stackOffset);
                    }
                    else if (registerCode == 3)
                    {
                        emitter.emitMovEbxEbpRelative(
                            variableIt->second.stackOffset);
                    }
                    else
                    {
                        throw std::runtime_error(
                            "K++ X86Backend: registro no soportado para LOAD.");
                    }
                }
                else if (registerCode == 0)
                {
                    // LOAD desde DATA: permanece absoluto.
                    const std::size_t patchOffset =
                        emitter.emitMovEaxMem32Placeholder();

                    variableLoadPatches.push_back({
                        currentFunctionName,
                        instruction.name,
                        patchOffset
                    });
                }
                else if (registerCode == 3)
                {
                    // LOAD desde DATA: permanece absoluto.
                    const std::size_t patchOffset =
                        emitter.emitMovEbxMem32Placeholder();

                    variableLoadPatches.push_back({
                        currentFunctionName,
                        instruction.name,
                        patchOffset
                    });
                }
                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: registro no soportado para LOAD.");
                }

                /*
                 * Diagnóstico del allocator.
                 */

                std::cerr
                    << "[ALLOC] "
                    << instruction.result
                    << " -> ";

                const auto temporaryIt =
                    temporaries.find(
                        instruction.result);

                if (temporaryIt !=
                    temporaries.end())
                {
                    const auto& location =
                        temporaryIt->second;

                    if (location.kind ==
                        TemporaryLocation::Kind::Register)
                    {
                        if (location.registerCode == 0)
                        {
                            std::cerr << "EAX";
                        }
                        else if (location.registerCode == 3)
                        {
                            std::cerr << "EBX";
                        }
                        else
                        {
                            std::cerr
                                << "REG("
                                << static_cast<int>(
                                       location.registerCode)
                                << ")";
                        }
                    }
                    else
                    {
                        std::cerr
                            << "MEM["
                            << location.dataOffset
                            << "]";
                    }
                }

                std::cerr << '\n';
            }

            // ------------------------------------------------
            // ADD
            // ------------------------------------------------

            else if (instruction.opcode ==
                     IROpcode::Add)
            {
                /*
                 * ADD %0, %1 -> %2
                 *
                 * Actualmente el asignador inicial de K++
                 * utiliza:
                 *
                 *     %0 -> EAX
                 *     %1 -> EBX
                 *
                 * Por tanto:
                 *
                 *     ADD EAX, EBX
                 *
                 * El resultado queda en EAX.
                 */

                const std::string left =
                    instruction.operand.substr(
                        0,
                        instruction.operand.find(","));

                const std::string right =
                    instruction.operand.substr(
                        instruction.operand.find(",") + 2);

                if (!hasTemporary(left))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: ADD usa temporal izquierdo inexistente: " +
                        left);
                }

                if (!hasTemporary(right))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: ADD usa temporal derecho inexistente: " +
                        right);
                }

                /*
                * ADD puede recibir un temporal que haya
                 * sido enviado a memoria por el allocator.
                 *
                 * Caso actual:
                 *
                 *     izquierdo = MEMORY
                 *     derecho   = EAX
                 *
                 * Necesitamos conservar el derecho antes
                 * de recargar el izquierdo:
                 *
                 *     PUSH EAX
                 *     MOV EAX, [izquierdo]
                 *     POP EBX
                 *     ADD EAX, EBX
                 *
                 * De esta forma:
                 *
                 *     EAX = izquierdo
                 *     EBX = derecho
                 */

                const auto leftIt =
                    temporaries.find(left);

                const auto rightIt =
                    temporaries.find(right);

                if (leftIt == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: ADD usa temporal izquierdo inexistente: " +
                        left);
                }

                if (rightIt == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: ADD usa temporal derecho inexistente: " +
                        right);
                }

                const auto leftKind =
                    leftIt->second.kind;

                const auto rightKind =
                    rightIt->second.kind;

                /*
                 * ------------------------------------------------
                 * CASO 1
                 *
                 * izquierdo = EAX
                 * derecho   = EBX
                 * ------------------------------------------------
                 */

                if (leftKind ==
                        TemporaryLocation::Kind::Register &&
                    leftIt->second.registerCode == 0 &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 3)
                {
                    emitter.emitAddEaxEbx();

                    /*
                     * ADD ha consumido ambos operandos.
                     * Sus valores temporales ya no son necesarios
                     * después de esta operación.
                     */
                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                /*
                 * ------------------------------------------------
                 * CASO 1B
                 *
                 * izquierdo = EBX
                 * derecho   = EAX
                 *
                 * ADD es conmutativo, por lo que transformamos:
                 *
                 *     EBX + EAX
                 *
                 * en:
                 *
                 *     EAX + EBX
                 *
                 * mediante:
                 *
                 *     PUSH EAX
                 *     MOV  EAX, EBX
                 *     POP  EBX
                 *     ADD  EAX, EBX
                 * ------------------------------------------------
                 */
                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Register &&
                    leftIt->second.registerCode == 3 &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 0)
                {
                    emitter.emitPushEax();
                    emitter.emitMovEaxEbx();
                    emitter.emitPopEbx();

                    emitter.emitAddEaxEbx();

                    /*
                     * ADD ha consumido ambos operandos.
                     */
                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                /*
                 * ------------------------------------------------
                 * CASO 2
                 *
                 * izquierdo = MEMORY
                 * derecho   = EAX
                 * ------------------------------------------------
                 */

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Memory &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 0)
                {
                    /*
                     * Conservamos el operando derecho.
                     */
                    emitter.emitPushEax();

                    /*
                     * Recargamos el operando izquierdo.
                     *
                     * La dirección absoluta se parcheará
                     * posteriormente mediante dataBase.
                     */
                    const std::size_t patchOffset =
                        emitMovEaxFramePlaceholder();

                    temporaryReloadPatches.push_back({
                        left,
                        patchOffset,
                        leftIt->second.dataOffset
                    });

                    /*
                     * Recuperamos el operando derecho:
                     *
                     *     EBX = derecho
                     */
                    emitter.emitPopEbx();

                    /*
                     * Ahora:
                     *
                     *     EAX = izquierdo
                     *     EBX = derecho
                     */
                    emitter.emitAddEaxEbx();

                    /*
                     * ADD ha consumido ambos operandos.
                     * Sus valores temporales ya no son necesarios
                     * después de esta operación.
                     */
                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: combinación de operandos ADD no soportada");
                }
                }


            // ------------------------------------------------
            // ========================================================
            // COMPARACIONES
            // ========================================================

            else if (
                instruction.opcode ==
                    IROpcode::CompareLess ||
                instruction.opcode ==
                    IROpcode::CompareLessEqual ||
                instruction.opcode ==
                    IROpcode::CompareEqual ||
                instruction.opcode ==
                    IROpcode::CompareNotEqual ||
                instruction.opcode ==
                    IROpcode::CompareGreaterEqual)
            {
                /*
                 * Comparaciones signed:
                 *
                 *     <   -> SETL
                 *     <=  -> SETLE
                 *     ==  -> SETE
                 *     !=  -> SETNE
                 *     >=  -> SETGE
                 *
                 * El resultado siempre queda en EAX:
                 *
                 *     EAX = 1  verdadero
                 *     EAX = 0  falso
                 *
                 * La secuencia base es:
                 *
                 *     CMP EAX, EBX
                 *     SETcc AL
                 *     MOVZX EAX, AL
                 *
                 * Se mantienen los mismos tres casos
                 * de operandos soportados por CMP_GT.
                 */

                const std::size_t comma =
                    instruction.operand.find(",");

                if (comma == std::string::npos)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: comparación tiene operandos inválidos");
                }

                const std::string left =
                    instruction.operand.substr(
                        0,
                        comma);

                const std::string right =
                    instruction.operand.substr(
                        comma + 2);

                if (!hasTemporary(left))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: comparación usa temporal izquierdo inexistente: " +
                        left);
                }

                if (!hasTemporary(right))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: comparación usa temporal derecho inexistente: " +
                        right);
                }

                const auto leftIt =
                    temporaries.find(left);

                const auto rightIt =
                    temporaries.find(right);

                if (leftIt == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: comparación usa temporal izquierdo inexistente: " +
                        left);
                }

                if (rightIt == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: comparación usa temporal derecho inexistente: " +
                        right);
                }

                const auto leftKind =
                    leftIt->second.kind;

                const auto rightKind =
                    rightIt->second.kind;


                /*
                 * Seleccionamos la operación SETcc.
                 */

                enum class CompareSet
                {
                    Less,
                    LessEqual,
                    Equal,
                    NotEqual,
                    GreaterEqual
                };

                CompareSet compareSet;

                switch (instruction.opcode)
                {
                    case IROpcode::CompareLess:
                        compareSet = CompareSet::Less;
                        break;

                    case IROpcode::CompareLessEqual:
                        compareSet = CompareSet::LessEqual;
                        break;

                    case IROpcode::CompareEqual:
                        compareSet = CompareSet::Equal;
                        break;

                    case IROpcode::CompareNotEqual:
                        compareSet = CompareSet::NotEqual;
                        break;

                    case IROpcode::CompareGreaterEqual:
                        compareSet = CompareSet::GreaterEqual;
                        break;

                    default:
                        throw std::runtime_error(
                            "K++ X86Backend: comparación interna inválida");
                }


                /*
                 * Función local para emitir SETcc.
                 *
                 * CMP ya dejó los flags correctos.
                 */

                const auto emitCompareSet =
                    [&]()
                    {
                        switch (compareSet)
                        {
                            case CompareSet::Less:
                                emitter.emitSetlAl();
                                break;

                            case CompareSet::LessEqual:
                                emitter.emitSetleAl();
                                break;

                            case CompareSet::Equal:
                                emitter.emitSeteAl();
                                break;

                            case CompareSet::NotEqual:
                                emitter.emitSetneAl();
                                break;

                            case CompareSet::GreaterEqual:
                                emitter.emitSetgeAl();
                                break;
                        }

                        emitter.emitMovzxEaxAl();
                    };


                /*
                 * ------------------------------------------------
                 * CASO 1
                 *
                 * izquierdo = EAX
                 * derecho   = EBX
                 * ------------------------------------------------
                 */

                if (
                    leftKind ==
                        TemporaryLocation::Kind::Register &&
                    leftIt->second.registerCode == 0 &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 3)
                {
                    emitter.emitCmpEaxEbx();

                    emitCompareSet();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }


                /*
                 * ------------------------------------------------
                 * CASO 2
                 *
                 * izquierdo = EBX
                 * derecho   = EAX
                 *
                 * Conservamos el derecho usando la pila.
                 * ------------------------------------------------
                 */

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Register &&
                    leftIt->second.registerCode == 3 &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 0)
                {
                    emitter.emitPushEax();
                    emitter.emitMovEaxEbx();
                    emitter.emitPopEbx();

                    emitter.emitCmpEaxEbx();

                    emitCompareSet();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }


                /*
                 * ------------------------------------------------
                 * CASO 3
                 *
                 * izquierdo = MEMORY
                 * derecho   = EAX
                 * ------------------------------------------------
                 */

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Memory &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 0)
                {
                    emitter.emitPushEax();

                    const std::size_t patchOffset =
                        emitMovEaxFramePlaceholder();

                    temporaryReloadPatches.push_back({
                        left,
                        patchOffset,
                        leftIt->second.dataOffset
                    });

                    emitter.emitPopEbx();

                    emitter.emitCmpEaxEbx();

                    emitCompareSet();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: combinación de operandos de comparación no soportada");
                }
            }


            // ------------------------------------------------
            // CMP_GREATER
            // ------------------------------------------------

            else if (instruction.opcode ==
                     IROpcode::CompareGreater)
            {
                /*
                 * CMP_GT %left, %right -> %result
                 *
                 * Resultado:
                 *
                 *     %result = 1    si left > right
                 *     %result = 0    en caso contrario
                 *
                 * La comparación es SIGNED.
                 *
                 * Secuencia base:
                 *
                 *     CMP EAX, EBX
                 *     SETG AL
                 *     MOVZX EAX, AL
                 *
                 * El resultado queda siempre en EAX.
                 */

                const std::size_t comma =
                    instruction.operand.find(",");

                if (comma == std::string::npos)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: CMP_GT tiene operandos inválidos");
                }

                const std::string left =
                    instruction.operand.substr(
                        0,
                        comma);

                const std::string right =
                    instruction.operand.substr(
                        comma + 2);

                if (!hasTemporary(left))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: CMP_GT usa temporal izquierdo inexistente: " +
                        left);
                }

                if (!hasTemporary(right))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: CMP_GT usa temporal derecho inexistente: " +
                        right);
                }

                const auto leftIt =
                    temporaries.find(left);

                const auto rightIt =
                    temporaries.find(right);

                if (leftIt == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: CMP_GT usa temporal izquierdo inexistente: " +
                        left);
                }

                if (rightIt == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: CMP_GT usa temporal derecho inexistente: " +
                        right);
                }

                const auto leftKind =
                    leftIt->second.kind;

                const auto rightKind =
                    rightIt->second.kind;


                /*
                 * ------------------------------------------------
                 * CASO 1
                 *
                 * izquierdo = EAX
                 * derecho   = EBX
                 *
                 * EAX > EBX
                 * ------------------------------------------------
                 */

                if (
                    leftKind ==
                        TemporaryLocation::Kind::Register &&
                    leftIt->second.registerCode == 0 &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 3)
                {
                    emitter.emitCmpEaxEbx();
                    emitter.emitSetgAl();
                    emitter.emitMovzxEaxAl();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }


                /*
                 * ------------------------------------------------
                 * CASO 2
                 *
                 * izquierdo = EBX
                 * derecho   = EAX
                 *
                 * Necesitamos transformar:
                 *
                 *     EBX > EAX
                 *
                 * en:
                 *
                 *     EAX = EBX
                 *     EBX = antiguo EAX
                 *
                 * usando la pila para conservar el derecho.
                 *
                 * ------------------------------------------------
                 */

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Register &&
                    leftIt->second.registerCode == 3 &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 0)
                {
                    emitter.emitPushEax();
                    emitter.emitMovEaxEbx();
                    emitter.emitPopEbx();

                    emitter.emitCmpEaxEbx();
                    emitter.emitSetgAl();
                    emitter.emitMovzxEaxAl();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }


                /*
                 * ------------------------------------------------
                 * CASO 3
                 *
                 * izquierdo = MEMORY
                 * derecho   = EAX
                 *
                 * Conservamos el derecho:
                 *
                 *     PUSH EAX
                 *
                 * Recargamos el izquierdo:
                 *
                 *     MOV EAX, [izquierdo]
                 *
                 * Recuperamos el derecho:
                 *
                 *     POP EBX
                 *
                 * Después:
                 *
                 *     CMP EAX, EBX
                 *     SETG AL
                 *     MOVZX EAX, AL
                 * ------------------------------------------------
                 */

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Memory &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 0)
                {
                    emitter.emitPushEax();

                    const std::size_t patchOffset =
                        emitMovEaxFramePlaceholder();

                    temporaryReloadPatches.push_back({
                        left,
                        patchOffset,
                        leftIt->second.dataOffset
                    });

                    emitter.emitPopEbx();

                    emitter.emitCmpEaxEbx();
                    emitter.emitSetgAl();
                    emitter.emitMovzxEaxAl();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                /*
                 * ------------------------------------------------
                 * CASO 4
                 *
                 * KPP_REBANADA31_CMPGT_CASO4
                 *
                 * izquierdo = MEMORY
                 * derecho   = EBX
                 *
                 * Si EAX conserva un temporal vivo ajeno a la
                 * comparación, se derrama primero a su slot
                 * del marco para no destruirlo.
                 *
                 * Recargamos el izquierdo:
                 *
                 *     MOV EAX, [izquierdo]
                 *
                 * Después:
                 *
                 *     CMP EAX, EBX
                 *     SETG AL
                 *     MOVZX EAX, AL
                 * ------------------------------------------------
                 */

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Memory &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 3)
                {
                    const std::string eaxTemporary =
                        temporaryInRegister(0);

                    if (!eaxTemporary.empty() &&
                        eaxTemporary != left &&
                        eaxTemporary != right)
                    {
                        spillTemporary(
                            eaxTemporary,
                            data,
                            temporarySpillPatches);
                    }

                    const std::size_t patchOffset =
                        emitMovEaxFramePlaceholder();

                    temporaryReloadPatches.push_back({
                        left,
                        patchOffset,
                        leftIt->second.dataOffset
                    });

                    emitter.emitCmpEaxEbx();
                    emitter.emitSetgAl();
                    emitter.emitMovzxEaxAl();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: combinación de operandos CMP_GT no soportada");
                }
            }



            // ------------------------------------------------
            // SUB
            // ------------------------------------------------
            //
            // SUB %left, %right -> %result
            //
            // SUB no es conmutativa.
            //
            // Casos soportados:
            //
            //     EAX - EBX
            //     EBX - EAX
            //     MEMORY - EAX
            //     EAX - MEMORY
            //     MEMORY - EBX
            //     EBX - MEMORY
            //
            // El resultado queda en EAX.
            // ------------------------------------------------


            // KPP_LOGICAL_L09_PATCH
          // AND / OR: admite operandos en registros o memoria.
          else if (
              instruction.opcode == IROpcode::LogicalAnd ||
              instruction.opcode == IROpcode::LogicalOr)
          {
              const std::size_t comma = instruction.operand.find(",");

              if (comma == std::string::npos)
              {
                  throw std::runtime_error(
                      "K++ X86Backend: operador logico requiere dos operandos");
              }

              const std::string left =
                  instruction.operand.substr(0, comma);
              const std::string right =
                  instruction.operand.substr(comma + 2);

              if (!hasTemporary(left) || !hasTemporary(right))
              {
                  throw std::runtime_error(
                      "K++ X86Backend: temporal inexistente en operador logico");
              }

              // Copiar las ubicaciones antes de modificar el mapa.
              const auto leftLocation = temporaries.at(left);
              const auto rightLocation = temporaries.at(right);

              const bool rightEax =
                  rightLocation.kind == TemporaryLocation::Kind::Register &&
                  rightLocation.registerCode == 0;

              // Si el derecho está en EAX, preservarlo antes de cargar
              // el izquierdo. Después recuperaremos ambos en orden.
            emitter.emitPushEbx();
              if (rightEax)
                  emitter.emitPushEax();

              // Cargar el izquierdo en EAX.
              if (leftLocation.kind == TemporaryLocation::Kind::Memory)
              {
                  const std::size_t patchOffset =
                      emitMovEaxFramePlaceholder();

                  temporaryReloadPatches.push_back({
                      left, patchOffset, leftLocation.dataOffset
                  });
              }
              else if (leftLocation.registerCode == 3)
              {
                  emitter.emitMovEaxEbx();
              }
              else if (leftLocation.registerCode != 0)
              {
                  throw std::runtime_error(
                      "K++ X86Backend: registro izquierdo no soportado en AND/OR");
              }

              // Normalizar el izquierdo a 0 o 1.
            emitter.emitCmpEaxImm32(0);
            emitter.emitSetneAl();
            emitter.emitMovzxEaxAl();

            if (rightEax)
            {
                // El derecho original está guardado en la pila.
                // Preservar el izquierdo normalizado en EBX y recuperar
                // el derecho en EAX.
                emitter.emitMovEbxEax();
                emitter.emitPopEax();

                emitter.emitCmpEaxImm32(0);
                emitter.emitSetneAl();
                emitter.emitMovzxEaxAl();
            }
            else
            {
                // Preservar el izquierdo mientras cargamos el derecho.
                emitter.emitPushEax();

                if (rightLocation.kind == TemporaryLocation::Kind::Memory)
                {
                    const std::size_t patchOffset =
                        emitMovEaxFramePlaceholder();

                    temporaryReloadPatches.push_back({
                        right, patchOffset, rightLocation.dataOffset
                    });
                }
                else if (rightLocation.registerCode == 3)
                {
                    emitter.emitMovEaxEbx();
                }
                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: ubicacion derecha no soportada en AND/OR");
                }

                emitter.emitCmpEaxImm32(0);
                emitter.emitSetneAl();
                emitter.emitMovzxEaxAl();

                // Izquierdo normalizado a EBX; derecho normalizado a EAX.
                emitter.emitPopEbx();
            }

            if (instruction.opcode == IROpcode::LogicalAnd)
                  emitter.emitAndEaxEbx();
              else
                  emitter.emitOrEaxEbx();
            emitter.emitPopEbx();

              freeTemporary(left);
              freeTemporary(right);
              bindTemporaryToEax(instruction.result);
          }

          else if (instruction.opcode == IROpcode::LogicalNot)
            {
                const std::string operand = instruction.operand;

                if (!hasTemporary(operand))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: NOT usa temporal inexistente: " +
                        operand);
                }

                const auto it = temporaries.find(operand);
                if (it == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: NOT usa temporal inexistente: " +
                        operand);
                }

                if (it->second.kind == TemporaryLocation::Kind::Register &&
                    it->second.registerCode == 0)
                {
                    emitter.emitCmpEaxImm32(0);
                }
                else if (
                    it->second.kind == TemporaryLocation::Kind::Register &&
                    it->second.registerCode == 3)
                {
                    emitter.emitMovEaxEbx();
                    emitter.emitCmpEaxImm32(0);
                }
                else if (it->second.kind == TemporaryLocation::Kind::Memory)
                {
                    const std::size_t patchOffset =
                        emitMovEaxFramePlaceholder();

                    temporaryReloadPatches.push_back({
                        operand,
                        patchOffset,
                        it->second.dataOffset
                    });

                    emitter.emitCmpEaxImm32(0);
                }
                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: ubicacion no soportada para NOT");
                }

                emitter.emitSeteAl();
                emitter.emitMovzxEaxAl();

                freeTemporary(operand);
                bindTemporaryToEax(instruction.result);
            }


            // ------------------------------------------------
            // SUB
            // ------------------------------------------------
            //
            // KPP_REBANADA3_SUB_PRESERVATION
            //
            // SUB %left, %right -> %result
            //
            // Antes de ejecutar SUB:
            //
            //     EAX y EBX se consideran registros de trabajo.
            //
            // Por tanto, ningún temporal vivo puede permanecer
            // exclusivamente en esos registros.
            //
            // Estrategia:
            //
            //     1. Derramar todos los temporales residentes
            //        en EAX y EBX al marco EBP.
            //
            //     2. Recargar el izquierdo en EAX.
            //
            //     3. Recargar el derecho en EBX.
            //
            //     4. Ejecutar SUB EAX, EBX.
            //
            // Los operandos originales permanecen respaldados
            // en memoria. No se eliminan del mapa porque pueden
            // tener usos posteriores.
            //
            // Resultado:
            //
            //     EAX = izquierdo - derecho
            //
            // ------------------------------------------------

            else if (instruction.opcode == IROpcode::Sub)
            {
                const std::size_t comma =
                    instruction.operand.find(",");

                if (comma == std::string::npos)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB requiere dos operandos");
                }

                const std::string left =
                    instruction.operand.substr(0, comma);

                const std::string right =
                    instruction.operand.substr(comma + 2);

                if (!hasTemporary(left))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB usa temporal izquierdo inexistente: " +
                        left);
                }

                if (!hasTemporary(right))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB usa temporal derecho inexistente: " +
                        right);
                }

                if (!frameActive)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB necesita un marco EBP activo "
                        "para preservar temporales.");
                }

                // ------------------------------------------------
                // FASE 1
                //
                // Reunir los temporales residentes en registros.
                //
                // No se modifica el mapa mientras se recorre.
                // ------------------------------------------------

                std::vector<std::string> registerTemporaries;

                for (const auto& entry : temporaries)
                {
                    if (entry.second.kind ==
                        TemporaryLocation::Kind::Register)
                    {
                        registerTemporaries.push_back(
                            entry.first);
                    }
                }

                // ------------------------------------------------
                // FASE 2
                //
                // Derramar cada temporal antes de destruir
                // el contenido de EAX o EBX.
                //
                // spillTemporary actualiza la ubicación del
                // temporal a MEMORY dentro de temporaries.
                // ------------------------------------------------

                for (const auto& temporary : registerTemporaries)
                {
                    const auto it =
                        temporaries.find(temporary);

                    if (it == temporaries.end())
                    {
                        continue;
                    }

                    if (it->second.kind ==
                        TemporaryLocation::Kind::Register)
                    {
                        spillTemporary(
                            temporary,
                            data,
                            temporarySpillPatches);
                    }
                }

                // ------------------------------------------------
                // FASE 3
                //
                // Verificar que ambos operandos quedaron en
                // slots EBP-relative.
                // ------------------------------------------------

                const auto leftIt =
                    temporaries.find(left);

                const auto rightIt =
                    temporaries.find(right);

                if (leftIt == temporaries.end() ||
                    rightIt == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB perdió un temporal "
                        "durante la preservación.");
                }

                if (leftIt->second.kind !=
                        TemporaryLocation::Kind::Memory ||
                    rightIt->second.kind !=
                        TemporaryLocation::Kind::Memory)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB no pudo preservar "
                        "ambos operandos en el marco.");
                }

                const std::int32_t leftOffset =
                    leftIt->second.dataOffset;

                const std::int32_t rightOffset =
                    rightIt->second.dataOffset;

                // ------------------------------------------------
                // FASE 4
                //
                // EAX = operando izquierdo.
                // ------------------------------------------------

                const std::size_t leftPatch =
                    emitMovEaxFramePlaceholder();

                temporaryReloadPatches.push_back({
                    left,
                    leftPatch,
                    leftOffset
                });

                // ------------------------------------------------
                // FASE 5
                //
                // EBX = operando derecho.
                // ------------------------------------------------

                const std::size_t rightPatch =
                    emitMovEbxFramePlaceholder();

                temporaryReloadPatches.push_back({
                    right,
                    rightPatch,
                    rightOffset
                });

                // ------------------------------------------------
                // FASE 6
                //
                // EAX = EAX - EBX
                //
                // No se ejecutan PUSH/POP para intercambiar
                // registros y, por tanto, no se destruye ningún
                // temporal que continúe registrado en EAX/EBX.
                // ------------------------------------------------

                emitter.emitSubEaxEbx();

                // ------------------------------------------------
                // FASE 7
                //
                // Los operandos originales permanecen en MEMORY.
                //
                // No llamar a freeTemporary(left/right):
                // pueden volver a utilizarse después de SUB.
                //
                // El resultado nuevo queda en EAX.
                // ------------------------------------------------

                bindTemporaryToEax(
                    instruction.result);
            }

            // ------------------------------------------------
            // MUL
            // ------------------------------------------------
            else if (instruction.opcode == IROpcode::Mul)
            {
                const std::size_t comma = instruction.operand.find(",");

                if (comma == std::string::npos)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: MUL tiene operandos invalidos.");
                }

                const std::string left =
                    instruction.operand.substr(0, comma);
                const std::string right =
                    instruction.operand.substr(comma + 2);

                if (!hasTemporary(left) || !hasTemporary(right))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: MUL usa un temporal inexistente.");
                }

                // Guardar los temporales que ocupan EAX y EBX.
                for (const std::uint8_t reg :
                     {std::uint8_t{0}, std::uint8_t{3}})
                {
                    const std::string temporary = temporaryInRegister(reg);

                    if (!temporary.empty())
                    {
                        spillTemporary(
                            temporary, data, temporarySpillPatches);
                    }
                }

                const auto leftIt = temporaries.find(left);
                const auto rightIt = temporaries.find(right);

                if (leftIt == temporaries.end() ||
                    rightIt == temporaries.end() ||
                    leftIt->second.kind != TemporaryLocation::Kind::Memory ||
                    rightIt->second.kind != TemporaryLocation::Kind::Memory)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: no se pudieron preparar los operandos de MUL.");
                }

                const std::size_t leftPatch =
                    emitMovEaxFramePlaceholder();

                temporaryReloadPatches.push_back({
                    left, leftPatch, leftIt->second.dataOffset
                });

                const std::size_t rightPatch =
                    emitMovEbxFramePlaceholder();

                temporaryReloadPatches.push_back({
                    right, rightPatch, rightIt->second.dataOffset
                });

                emitter.emitMulEaxEbx();

                freeTemporary(left);
                freeTemporary(right);
                bindTemporaryToEax(instruction.result);
            }

            // ------------------------------------------------
            // DIV
            // ------------------------------------------------
            else if (instruction.opcode == IROpcode::Div)
            {
                const std::size_t comma =
                    instruction.operand.find(",");

                if (comma == std::string::npos)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: DIV tiene operandos invalidos.");
                }

                const std::string left =
                    instruction.operand.substr(0, comma);
                const std::string right =
                    instruction.operand.substr(comma + 2);

                if (!hasTemporary(left) || !hasTemporary(right))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: DIV usa un temporal inexistente.");
                }

                // Guardar cualquier temporal que ocupe EAX o EBX.
                for (const std::uint8_t reg :
                     {std::uint8_t{0}, std::uint8_t{3}})
                {
                    const std::string temporary = temporaryInRegister(reg);

                    if (!temporary.empty())
                    {
                        spillTemporary(
                            temporary, data, temporarySpillPatches);
                    }
                }

                const auto leftIt = temporaries.find(left);
                const auto rightIt = temporaries.find(right);

                if (leftIt == temporaries.end() ||
                    rightIt == temporaries.end() ||
                    leftIt->second.kind != TemporaryLocation::Kind::Memory ||
                    rightIt->second.kind != TemporaryLocation::Kind::Memory)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: no se pudieron preparar los operandos de DIV.");
                }

                // EAX = operando izquierdo.
                const std::size_t leftPatch =
                    emitMovEaxFramePlaceholder();

                temporaryReloadPatches.push_back({
                    left, leftPatch, leftIt->second.dataOffset
                });

                // EBX = divisor.
                const std::size_t rightPatch =
                    emitMovEbxFramePlaceholder();

                temporaryReloadPatches.push_back({
                    right, rightPatch, rightIt->second.dataOffset
                });

                // División entera con signo:
                // EDX:EAX / EBX -> cociente en EAX.
                emitter.emitCdq();
                emitter.emitIdivEbx();

                freeTemporary(left);
                freeTemporary(right);
                bindTemporaryToEax(instruction.result);
            }


            // ------------------------------------------------
            // MOD
            // ------------------------------------------------
            else if (instruction.opcode == IROpcode::Mod)
            {
                const std::size_t comma =
                    instruction.operand.find(",");

                if (comma == std::string::npos)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: MOD tiene operandos invalidos.");
                }

                const std::string left =
                    instruction.operand.substr(0, comma);
                const std::string right =
                    instruction.operand.substr(comma + 2);

                if (!hasTemporary(left) || !hasTemporary(right))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: MOD usa un temporal inexistente.");
                }

                // Guardar cualquier temporal que ocupe EAX o EBX.
                for (const std::uint8_t reg :
                     {std::uint8_t{0}, std::uint8_t{3}})
                {
                    const std::string temporary = temporaryInRegister(reg);

                    if (!temporary.empty())
                    {
                        spillTemporary(
                            temporary, data, temporarySpillPatches);
                    }
                }

                const auto leftIt = temporaries.find(left);
                const auto rightIt = temporaries.find(right);

                if (leftIt == temporaries.end() ||
                    rightIt == temporaries.end() ||
                    leftIt->second.kind != TemporaryLocation::Kind::Memory ||
                    rightIt->second.kind != TemporaryLocation::Kind::Memory)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: no se pudieron preparar los operandos de MOD.");
                }

                // EAX = dividendo.
                const std::size_t leftPatch =
                    emitMovEaxFramePlaceholder();

                temporaryReloadPatches.push_back({
                    left, leftPatch, leftIt->second.dataOffset
                });

                // EBX = divisor.
                const std::size_t rightPatch =
                    emitMovEbxFramePlaceholder();

                temporaryReloadPatches.push_back({
                    right, rightPatch, rightIt->second.dataOffset
                });

                // IDIV deja el resto con signo en EDX.
                emitter.emitCdq();
                emitter.emitIdivEbx();
                emitter.emitMovEaxEdx();

                freeTemporary(left);
                freeTemporary(right);
                bindTemporaryToEax(instruction.result);
            }

            // ------------------------------------------------
            // JUMP IF ZERO
            // ------------------------------------------------

            else if (instruction.opcode ==
                     IROpcode::JumpIfZero)
            {
                /*
                 * JUMP_IF_ZERO %condition -> LABEL
                 *
                 * La condición generada por las comparaciones
                 * queda actualmente en EAX.
                 *
                 * Generamos:
                 *
                 *     CMP EAX, 0
                 *     JE  LABEL
                 *
                 * El desplazamiento de JE se parchea después,
                 * cuando todas las etiquetas ya tienen posición.
                 */

                if (instruction.operand.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: JUMP_IF_ZERO sin condición");
                }

                if (instruction.name.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: JUMP_IF_ZERO sin etiqueta destino");
                }

                if (!hasTemporary(
                        instruction.operand))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: JUMP_IF_ZERO usa un temporal inexistente: " +
                        instruction.operand);
                }

                const std::uint8_t reg =
                    getTemporaryRegister(
                        instruction.operand);

                if (reg != 0)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: JUMP_IF_ZERO actualmente requiere EAX");
                }

                emitter.emitCmpEaxImm32(0);

                const std::size_t patchOffset =
                    emitter.emitJeRel32Placeholder();

                controlFlowPatches.push_back({
                    makeScopedLabel(instruction.name),
                    patchOffset
                });
            }


            // ------------------------------------------------
            // JUMP
            // ------------------------------------------------

            else if (instruction.opcode ==
                     IROpcode::Jump)
            {
                /*
                 * JUMP LABEL
                 *
                 * Generamos:
                 *
                 *     JMP LABEL
                 *
                 * El desplazamiento relativo se parchea
                 * posteriormente, cuando todas las etiquetas
                 * ya tienen posición.
                 */

                if (instruction.name.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: JUMP sin etiqueta destino");
                }

                const std::size_t patchOffset =
                    emitter.emitJmpRel32Placeholder();

                controlFlowPatches.push_back({
                    makeScopedLabel(instruction.name),
                    patchOffset
                });
            }


            // ------------------------------------------------
            // LABEL
            // ------------------------------------------------

            else if (instruction.opcode ==
                     IROpcode::Label)
            {
                /*
                 * LABEL no genera bytes.
                 *
                 * Simplemente registra la posición actual
                 * del código máquina para que los saltos
                 * puedan ser parcheados posteriormente.
                 */

                if (instruction.name.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: LABEL sin nombre");
                }

                const std::string scopedLabel =
                    makeScopedLabel(instruction.name);

                if (labels.find(scopedLabel) !=
                    labels.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: LABEL duplicado: " +
                        scopedLabel);
                }

                labels[scopedLabel] =
                    emitter.size();
            }





            // ------------------------------------------------
            // PRINT
            // ------------------------------------------------
            else if (instruction.opcode ==
                    IROpcode::Print)
            {
                if (!hasTemporary(
                        instruction.operand))
                {
                    throw std::runtime_error(
                        "PRINT usa un temporal inexistente: " +
                        instruction.operand);
                }

                const std::uint8_t reg =
                    getTemporaryRegister(
                        instruction.operand);

                if (reg == 0)
                {
                    // MOV EDX, EAX
                    emitter.emitMovEdxEax();
                }
                else if (reg == 3)
                {
                    // MOV EDX, EBX
                    emitter.emitMovEdxEbx();
                }
                else
                {
                    throw std::runtime_error(
                        "PRINT: registro temporal no soportado.");
                }

                const std::uint32_t y =
                    40u +
                    static_cast<std::uint32_t>(
                        printIndex * 20u);

                runtime.emitPrintFromEdx(
                    emitter,
                    y);

                ++printIndex;
            }


            // ------------------------------------------------
            // CALL
            // ------------------------------------------------
            //
            // CALL suma -> %0
            //
            // x86:
            //
            //     E8 rel32
            //
            // El resultado de una función K++ se devuelve
            // mediante EAX.
            // ------------------------------------------------


            // PRINT INTEGER
            else if (instruction.opcode ==
                     IROpcode::PrintInteger)
            {
                if (!hasTemporary(instruction.operand)) {
                    throw std::runtime_error(
                        "PRINT_INT usa un temporal inexistente: " +
                        instruction.operand);
                }

                const std::uint8_t reg =
                    getTemporaryRegister(instruction.operand);

                if (reg == 3) {
                    emitter.emitMovEaxEbx();
                } else if (reg != 0) {
                    throw std::runtime_error(
                        "PRINT_INT: registro temporal no soportado.");
                }

                const std::uint32_t y =
                    40u + static_cast<std::uint32_t>(
                        printIndex * 30u);

                const std::size_t patch =
                    runtime.emitPrintInteger(emitter, y);

                integerBufferPatches.push_back(patch);
                ++printIndex;
            }


            // ------------------------------------------------
            // KPP_FILE_READ_HANDLER_PHASE4
            //
            // FILE_READ "ruta", array@id,
            //           capacity=N -> %resultado
            //
            // ABI INTERNA:
            //
            //     EAX = dirección de ruta
            //     EBX = dirección del array
            //     ECX = capacidad del array
            //
            // RETORNO:
            //
            //     EAX = bytes leídos
            //     EAX = -1 error
            //
            // IMPORTANTE:
            //
            // Las direcciones absolutas de ruta y array
            // permanecen pendientes para la fase 5.
            //
            // La capacidad se transporta como inmediato.
            //
            // No existe temporal de cantidad en FILE_READ.
            // ------------------------------------------------

else if (instruction.opcode ==
                     IROpcode::FileRead)
            {
                // ------------------------------------------------
                // VALIDACIÓN DEL CONTEXTO
                // ------------------------------------------------

                if (!frameActive)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_READ fuera "
                        "de un marco activo");
                }

                if (instruction.name.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_READ sin ruta");
                }

                if (instruction.name.find('\0') !=
                    std::string::npos)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: la ruta FILE_READ "
                        "contiene NUL interno");
                }

                if (instruction.operand.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_READ sin "
                        "identidad de array");
                }

                if (instruction.result.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_READ sin "
                        "temporal de retorno");
                }

                if (instruction.value <= 0)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: capacidad FILE_READ "
                        "debe ser al menos 1");
                }

                // ------------------------------------------------
                // LOCALIZAR EL ARRAY
                // ------------------------------------------------

                const auto arrayIt =
                    variables.find(instruction.operand);

                if (arrayIt == variables.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: array FILE_READ "
                        "no declarado: " +
                        instruction.operand);
                }

                const VariableLocation& arrayLocation =
                    arrayIt->second;

                if (arrayLocation.isStackLocal)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_READ requiere "
                        "un array en DATA, no una variable "
                        "escalar local");
                }

                if (arrayLocation.arrayCapacity == 0)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: capacidad de array "
                        "no registrada: " +
                        instruction.operand);
                }

                if (arrayLocation.arrayCapacity !=
                    static_cast<std::size_t>(
                        instruction.value))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: la capacidad FILE_READ "
                        "no coincide con DECLARE: " +
                        instruction.operand);
                }

                // ------------------------------------------------
                // PRESERVAR TEMPORALES VIVOS — L15
                //
                // Derramar todos los temporales residentes
                // en EAX y EBX antes de preparar argumentos.
                //
                // FILE_READ no tiene temporal de cantidad.
                // No se consulta auxiliary.
                // ------------------------------------------------

                {
                    std::vector<std::string> liveRegisters;

                    for (const auto& entry : temporaries)
                    {
                        if (entry.second.kind ==
                            TemporaryLocation::Kind::Register)
                        {
                            liveRegisters.push_back(
                                entry.first);
                        }
                    }

                    for (const auto& temporary :
                         liveRegisters)
                    {
                        const auto it =
                            temporaries.find(temporary);

                        if (it != temporaries.end() &&
                            it->second.kind ==
                                TemporaryLocation::Kind::Register)
                        {
                            spillTemporary(
                                temporary,
                                data,
                                temporarySpillPatches);
                        }
                    }
                }

                // ------------------------------------------------
                // COPIAR RUTA A DATA
                //
                // Se registra el offset relativo.
                // La dirección absoluta se resuelve en fase 5.
                // ------------------------------------------------

                const std::size_t pathDataOffset =
                    data.size();

                for (const char c : instruction.name)
                {
                    data.push_back(
                        static_cast<std::uint8_t>(
                            static_cast<unsigned char>(c)));
                }

                // Terminador C.
                data.push_back(0);

                // ------------------------------------------------
                // EAX = DIRECCIÓN DE RUTA
                //
                // B8 imm32
                // El imm32 queda pendiente de fase 5.
                // ------------------------------------------------

                const std::size_t pathAddressPatch =
                    emitter.size() + 1;

                emitter.emitMovEaxImm32(0);

                // ------------------------------------------------
                // EBX = DIRECCIÓN DEL ARRAY
                //
                // BB imm32
                // El imm32 queda pendiente de fase 5.
                // ------------------------------------------------

                const std::size_t arrayAddressPatch =
                    emitter.size() + 1;

                emitter.emitMovRegImm32(
                    0xBB,
                    0);

                // ------------------------------------------------
                // ECX = CAPACIDAD DEL ARRAY
                //
                // B9 imm32
                //
                // La capacidad procede de DECLARE y ya fue
                // contrastada contra instruction.value.
                //
                // No se carga desde EBP.
                // No existe temporal de cantidad.
                // ------------------------------------------------

                emitter.emitMovRegImm32(
                    0xB9,
                    static_cast<std::uint32_t>(
                        arrayLocation.arrayCapacity));

                // ------------------------------------------------
                // CALL FILE_READ_RUNTIME
                //
                // fileReadEntry fue generado en fase 2.
                // El destino está dentro del mismo buffer x86.
                // ------------------------------------------------

                emitter.emitByte(0xE8);

                const std::size_t runtimeCallPatch =
                    emitter.emitRel32Placeholder();

                emitter.patchRel32(
                    runtimeCallPatch,
                    fileIoRuntimeResult.fileReadEntry);

                // ------------------------------------------------
                // RETORNO DEL RUNTIME EN EAX
                //
                // EAX:
                //     >= 0 : bytes leídos
                //     -1   : error
                // ------------------------------------------------

                bindTemporaryToEax(
                    instruction.result);

                // ------------------------------------------------
                // REGISTRAR DIRECCIONES PARA FASE 5
                //
                // No resolver aquí dataBase + offset.
                // FILE_READ y FILE_WRITE serán resueltos
                // conjuntamente en la fase 5.
                // ------------------------------------------------

                fileReadPatches.push_back({
                    currentFunctionName,
                    instruction.operand,
                    pathDataOffset,
                    pathAddressPatch,
                    arrayAddressPatch,
                    arrayLocation.arrayCapacity
                });
            }


            // ------------------------------------------------
            // KPP_FILE_WRITE_HANDLER_PHASE3
            //
            // FILE_WRITE "ruta", array@id, %cantidad,
            //            capacity=N -> %resultado
            //
            // ABI INTERNA:
            //
            //     EAX = dirección de ruta
            //     EBX = dirección del array
            //     ECX = cantidad de celdas
            //     EDX = capacidad del array
            //
            // RETORNO:
            //
            //     EAX = 0  éxito
            //     EAX = -1 error
            //
            // IMPORTANTE:
            //
            // Las direcciones absolutas de ruta y array
            // permanecen pendientes para la fase 5.
            //
            // No se modifica main.cpp.
            // ------------------------------------------------

else if (instruction.opcode ==
                     IROpcode::FileWrite)
            {
                // ------------------------------------------------
                // VALIDACIÓN DEL CONTEXTO
                // ------------------------------------------------

                if (!frameActive)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_WRITE fuera "
                        "de un marco activo");
                }

                if (instruction.name.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_WRITE sin ruta");
                }

                if (instruction.name.find('\0') !=
                    std::string::npos)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: la ruta FILE_WRITE "
                        "contiene NUL interno");
                }

                if (instruction.operand.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_WRITE sin "
                        "identidad de array");
                }

                if (instruction.result.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_WRITE sin "
                        "temporal de retorno");
                }

                if (instruction.auxiliary.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_WRITE sin "
                        "temporal de cantidad");
                }

                if (instruction.value <= 0)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: capacidad FILE_WRITE "
                        "debe ser al menos 1");
                }

                // ------------------------------------------------
                // LOCALIZAR EL ARRAY
                // ------------------------------------------------

                const auto arrayIt =
                    variables.find(instruction.operand);

                if (arrayIt == variables.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: array FILE_WRITE "
                        "no declarado: " +
                        instruction.operand);
                }

                const VariableLocation& arrayLocation =
                    arrayIt->second;

                if (arrayLocation.isStackLocal)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: FILE_WRITE requiere "
                        "un array en DATA, no una variable "
                        "escalar local");
                }

                if (arrayLocation.arrayCapacity == 0)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: capacidad de array "
                        "no registrada: " +
                        instruction.operand);
                }

                if (arrayLocation.arrayCapacity !=
                    static_cast<std::size_t>(
                        instruction.value))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: la capacidad FILE_WRITE "
                        "no coincide con DECLARE: " +
                        instruction.operand);
                }

                // ------------------------------------------------
                // VALIDAR TEMPORAL DE CANTIDAD
                // ------------------------------------------------

                if (!hasTemporary(instruction.auxiliary))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: temporal de cantidad "
                        "FILE_WRITE no encontrado: " +
                        instruction.auxiliary);
                }

                // ------------------------------------------------
                // PRESERVAR TEMPORALES VIVOS — L15
                //
                // Derramar todos los temporales residentes
                // en EAX y EBX antes de preparar argumentos.
                // ------------------------------------------------

                {
                    std::vector<std::string> liveRegisters;

                    for (const auto& entry : temporaries)
                    {
                        if (entry.second.kind ==
                            TemporaryLocation::Kind::Register)
                        {
                            liveRegisters.push_back(
                                entry.first);
                        }
                    }

                    for (const auto& temporary :
                         liveRegisters)
                    {
                        const auto it =
                            temporaries.find(temporary);

                        if (it != temporaries.end() &&
                            it->second.kind ==
                                TemporaryLocation::Kind::Register)
                        {
                            spillTemporary(
                                temporary,
                                data,
                                temporarySpillPatches);
                        }
                    }
                }

                // ------------------------------------------------
                // RECUPERAR EL SLOT DEL TEMPORAL DE CANTIDAD
                // ------------------------------------------------

                const auto countIt =
                    temporaries.find(
                        instruction.auxiliary);

                if (countIt == temporaries.end() ||
                    countIt->second.kind !=
                        TemporaryLocation::Kind::Memory)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: no se pudo preservar "
                        "la cantidad FILE_WRITE en EBP");
                }

                if (countIt->second.dataOffset >= 0 ||
                    (countIt->second.dataOffset % 4) != 0)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: displacement EBP "
                        "inválido para FILE_WRITE");
                }

                // ------------------------------------------------
                // COPIAR RUTA A DATA
                //
                // Se registra el offset relativo.
                // La dirección absoluta se resuelve en fase 5.
                // ------------------------------------------------

                const std::size_t pathDataOffset =
                    data.size();

                for (const char c : instruction.name)
                {
                    data.push_back(
                        static_cast<std::uint8_t>(
                            static_cast<unsigned char>(c)));
                }

                // Terminador C.
                data.push_back(0);

                // ------------------------------------------------
                // EAX = DIRECCIÓN DE RUTA
                //
                // B8 imm32
                // El imm32 queda pendiente de fase 5.
                // ------------------------------------------------

                const std::size_t pathAddressPatch =
                    emitter.size() + 1;

                emitter.emitMovEaxImm32(0);

                // ------------------------------------------------
                // EBX = DIRECCIÓN DEL ARRAY
                //
                // BB imm32
                // El imm32 queda pendiente de fase 5.
                // ------------------------------------------------

                const std::size_t arrayAddressPatch =
                    emitter.size() + 1;

                emitter.emitMovRegImm32(
                    0xBB,
                    0);

                // ------------------------------------------------
                // ECX = [EBP + displacement]
                //
                // MOV ECX, [EBP + disp32]
                //
                // Codificación:
                //     8B 8D disp32
                //
                // El desplazamiento EBP-relative ya es conocido.
                // ------------------------------------------------

                emitter.emitByte(0x8B);
                emitter.emitByte(0x8D);

                const std::uint32_t countDisplacement =
                    static_cast<std::uint32_t>(
                        countIt->second.dataOffset);

                for (unsigned shift = 0;
                     shift < 32;
                     shift += 8)
                {
                    emitter.emitByte(
                        static_cast<std::uint8_t>(
                            (countDisplacement >> shift) &
                            0xFFu));
                }

                // ------------------------------------------------
                // EDX = CAPACIDAD DEL ARRAY
                //
                // BA imm32
                // ------------------------------------------------

                emitter.emitMovRegImm32(
                    0xBA,
                    static_cast<std::uint32_t>(
                        arrayLocation.arrayCapacity));

                // ------------------------------------------------
                // CALL FILE_WRITE_RUNTIME
                //
                // fileWriteEntry fue generado en fase 2.
                // El destino está dentro del mismo buffer x86.
                // ------------------------------------------------

                emitter.emitByte(0xE8);

                const std::size_t runtimeCallPatch =
                    emitter.emitRel32Placeholder();

                emitter.patchRel32(
                    runtimeCallPatch,
                    fileIoRuntimeResult.fileWriteEntry);

                // ------------------------------------------------
                // LIBERAR EL TEMPORAL DE CANTIDAD
                //
                // Su valor ya fue transferido a ECX.
                // ------------------------------------------------

                freeTemporary(
                    instruction.auxiliary);

                // ------------------------------------------------
                // RETORNO DEL RUNTIME EN EAX
                // ------------------------------------------------

                bindTemporaryToEax(
                    instruction.result);

                // ------------------------------------------------
                // REGISTRAR DIRECCIONES PARA FASE 5
                //
                // No resolver aquí dataBase + offset.
                // ------------------------------------------------

                fileWritePatches.push_back({
                    currentFunctionName,
                    instruction.operand,
                    pathDataOffset,
                    pathAddressPatch,
                    arrayAddressPatch,
                    arrayLocation.arrayCapacity
                });
            }


else if (instruction.opcode ==
                     IROpcode::Call)
            {
                if (instruction.name.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: CALL sin función destino");
                }

                if (instruction.result.empty())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: CALL sin temporal resultado");
                }

                // ------------------------------------------------
                // ARGUMENTOS DE CALL
                //
                // Convención inicial de K++:
                //
                //     argumento 0 -> EAX
                //     argumento 1 -> EBX
                //     argumento 2.. -> pila del llamador
                //         (CICLO 4; el llamado los recoge en
                //         [EBP+8], [EBP+12], … y el llamador
                //         limpia la pila tras el CALL)
                //
                // Los argumentos pueden encontrarse:
                //
                //     EAX
                //     EBX
                //     MEMORY
                //
                // Antes de CALL los normalizamos físicamente.
                // ------------------------------------------------

                // Preservar los temporales residentes en registros
                // antes de preparar los argumentos de CALL.
                {
                    std::vector<std::string> liveRegisters;

                    for (const auto& entry : temporaries)
                    {
                        if (entry.second.kind == TemporaryLocation::Kind::Register)
                            liveRegisters.push_back(entry.first);
                    }

                    for (const auto& temporary : liveRegisters)
                    {
                        auto it = temporaries.find(temporary);
                        if (it != temporaries.end() &&
                            it->second.kind == TemporaryLocation::Kind::Register)
                        {
                            spillTemporary(temporary, data, temporarySpillPatches);
                        }
                    }
                }

                // CICLO 4: la lista de argumentos puede
                // tener cualquier longitud. Los argumentos
                // 0 y 1 siguen llamándose left y right para
                // la normalización histórica; la lista
                // completa vive en `arguments`.
                std::vector<std::string> arguments;

                std::size_t extraStackArguments = 0;

                std::string left;
                std::string right;

                if (!instruction.operand.empty())
                {
                    std::size_t argStart = 0;

                    while (true)
                    {
                        const std::size_t comma =
                            instruction.operand.find(
                                ',', argStart);

                        std::string argument =
                            instruction.operand.substr(
                                argStart,
                                (comma == std::string::npos)
                                    ? std::string::npos
                                    : comma - argStart);

                        while (!argument.empty() &&
                               argument.front() == ' ')
                        {
                            argument.erase(
                                argument.begin());
                        }

                        while (!argument.empty() &&
                               argument.back() == ' ')
                        {
                            argument.pop_back();
                        }

                        if (argument.empty())
                        {
                            throw std::runtime_error(
                                "K++ X86Backend: CALL argumento vacío");
                        }

                        if (temporaries.find(argument) ==
                            temporaries.end())
                        {
                            throw std::runtime_error(
                                "K++ X86Backend: CALL argumento no encontrado: " +
                                argument);
                        }

                        arguments.push_back(argument);

                        if (comma == std::string::npos)
                        {
                            break;
                        }

                        argStart = comma + 1;
                    }

                    left = arguments[0];

                    if (arguments.size() > 1)
                    {
                        right = arguments[1];
                    }

                    // Argumentos que viajarán por la pila
                    // (índice >= 2); este llamador los retira
                    // tras el CALL.
                    extraStackArguments =
                        (arguments.size() > 2)
                            ? arguments.size() - 2
                            : 0;

                    const auto leftIt =
                        temporaries.find(left);

                    auto emitLoadMemoryToEax =
                        [&](const std::string& temporary,
                            std::int32_t dataOffset)
                    {
                        const std::size_t reloadPatchOffset =
                            emitMovEaxFramePlaceholder();

                        temporaryReloadPatches.push_back({
                            temporary,
                            reloadPatchOffset,
                            dataOffset
                        });
                    };

                    // ------------------------------------------------
                    // CICLO 4 — ARGUMENTOS POR PILA (índice >= 2)
                    //
                    // Se empujan en orden inverso (el último
                    // primero) ANTES de normalizar EAX/EBX, de
                    // modo que al ejecutarse CALL el argumento
                    // 2 quede en [EBP+8] del llamado, el 3 en
                    // [EBP+12], etc. Invariante: el derrame
                    // universal previo dejó todos los temporales
                    // en memoria; la lanzadera es EAX y la
                    // normalización posterior usa empujes
                    // transitorios balanceados que no alteran
                    // esta zona de la pila.
                    // ------------------------------------------------

                    for (std::size_t argIndex = arguments.size();
                         argIndex-- > 2;)
                    {
                        const std::string& argument =
                            arguments[argIndex];

                        const auto& location =
                            temporaries.at(argument);

                        if (location.kind ==
                            TemporaryLocation::Kind::Memory)
                        {
                            emitLoadMemoryToEax(
                                argument,
                                location.dataOffset);
                        }
                        else if (location.registerCode == 0)
                        {
                            // Ya está en EAX.
                        }
                        else if (location.registerCode == 3)
                        {
                            emitter.emitMovEaxEbx();
                        }
                        else
                        {
                            throw std::runtime_error(
                                "K++ X86Backend: registro de argumento no soportado");
                        }

                        emitter.emitPushEax();
                    }

                    const auto& leftLocation =
                        leftIt->second;

                    if (right.empty())
                    {
                        // ----------------------------------------
                        // Un solo argumento -> EAX
                        // ----------------------------------------

                        if (leftLocation.kind ==
                            TemporaryLocation::Kind::Memory)
                        {
                            emitLoadMemoryToEax(
                                left,
                                leftLocation.dataOffset);
                        }
                        else if (
                            leftLocation.registerCode == 0)
                        {
                            // Ya está en EAX.
                        }
                        else if (
                            leftLocation.registerCode == 3)
                        {
                            emitter.emitMovEaxEbx();
                        }
                        else
                        {
                            throw std::runtime_error(
                                "K++ X86Backend: registro de argumento no soportado");
                        }
                    }
                    else
                    {
                        const auto& rightLocation =
                            temporaries.at(right);

                        const auto leftKind =
                            leftLocation.kind;

                        const auto rightKind =
                            rightLocation.kind;

                        const bool leftEax =
                            leftKind ==
                                TemporaryLocation::Kind::Register &&
                            leftLocation.registerCode == 0;

                        const bool leftEbx =
                            leftKind ==
                                TemporaryLocation::Kind::Register &&
                            leftLocation.registerCode == 3;

                        const bool leftMemory =
                            leftKind ==
                                TemporaryLocation::Kind::Memory;

                        const bool rightEax =
                            rightKind ==
                                TemporaryLocation::Kind::Register &&
                            rightLocation.registerCode == 0;

                        const bool rightEbx =
                            rightKind ==
                                TemporaryLocation::Kind::Register &&
                            rightLocation.registerCode == 3;

                        const bool rightMemory =
                            rightKind ==
                                TemporaryLocation::Kind::Memory;

                        // ----------------------------------------
                        // EAX , EBX
                        // ----------------------------------------

                        if (leftEax && rightEbx)
                        {
                            // Ya preparados.
                        }

                        // ----------------------------------------
                        // EBX , EAX
                        // ----------------------------------------

                        else if (leftEbx && rightEax)
                        {
                            emitter.emitPushEax();
                            emitter.emitMovEaxEbx();
                            emitter.emitPopEbx();
                        }

                        // ----------------------------------------
                        // EAX , MEMORY
                        // ----------------------------------------

                        else if (leftEax && rightMemory)
                        {
                            // Guardamos argumento 0.
                            emitter.emitPushEax();

                            // Cargamos argumento 1.
                            emitLoadMemoryToEax(
                                right,
                                rightLocation.dataOffset);

                            // EBX = argumento 0.
                            emitter.emitPopEbx();

                            // Intercambiamos:
                            //
                            // EAX = argumento 0
                            // EBX = argumento 1
                            emitter.emitPushEax();
                            emitter.emitMovEaxEbx();
                            emitter.emitPopEbx();
                        }

                        // ----------------------------------------
                        // EBX , MEMORY
                        // ----------------------------------------

                        else if (leftEbx && rightMemory)
                        {
                            // Conservamos argumento 0 en EAX.
                            emitter.emitMovEaxEbx();
                            emitter.emitPushEax();

                            // Cargamos argumento 1.
                            emitLoadMemoryToEax(
                                right,
                                rightLocation.dataOffset);

                            // EBX = argumento 0.
                            emitter.emitPopEbx();

                            // Intercambiamos.
                            emitter.emitPushEax();
                            emitter.emitMovEaxEbx();
                            emitter.emitPopEbx();
                        }

                        // ----------------------------------------
                        // MEMORY , EAX
                        // ----------------------------------------

                        else if (leftMemory && rightEax)
                        {
                            // Guardamos argumento 1.
                            emitter.emitPushEax();

                            // EAX = argumento 0.
                            emitLoadMemoryToEax(
                                left,
                                leftLocation.dataOffset);

                            // EBX = argumento 1.
                            emitter.emitPopEbx();
                        }

                        // ----------------------------------------
                        // MEMORY , EBX
                        // ----------------------------------------

                        else if (leftMemory && rightEbx)
                        {
                            // EAX = argumento 0.
                            emitLoadMemoryToEax(
                                left,
                                leftLocation.dataOffset);

                            // EBX ya contiene argumento 1.
                        }

                        // ----------------------------------------
                        // MEMORY , MEMORY
                        // ----------------------------------------

                        else if (leftMemory && rightMemory)
                        {
                            // EAX = argumento 0.
                            emitLoadMemoryToEax(
                                left,
                                leftLocation.dataOffset);

                            // Conservamos argumento 0.
                            emitter.emitPushEax();

                            // EAX = argumento 1.
                            emitLoadMemoryToEax(
                                right,
                                rightLocation.dataOffset);

                            // EBX = argumento 0.
                            emitter.emitPopEbx();

                            // Intercambiamos.
                            emitter.emitPushEax();
                            emitter.emitMovEaxEbx();
                            emitter.emitPopEbx();
                        }

                        else
                        {
                            throw std::runtime_error(
                                "K++ X86Backend: combinación de argumentos CALL no soportada");
                        }
                    }

                    // ------------------------------------------------
                    // Los argumentos quedan consumidos por CALL.
                    // ------------------------------------------------

                    freeTemporary(left);

                    if (!right.empty())
                    {
                        freeTemporary(right);
                    }

                    // CICLO 4: también quedan consumidos los
                    // argumentos que viajaron por la pila.
                    for (std::size_t argIndex = 2;
                         argIndex < arguments.size();
                         ++argIndex)
                    {
                        freeTemporary(arguments[argIndex]);
                    }
                }

                // ------------------------------------------------
                // CALL rel32
                // ------------------------------------------------

                emitter.emitByte(0xE8);

                const std::size_t patchOffset =
                    emitter.emitRel32Placeholder();

                functionCallPatches.push_back({
                    instruction.name,
                    patchOffset
                });

                // CICLO 4: el llamador retira de la pila los
                // argumentos que empujó (índice >= 2). EAX
                // conserva el resultado de la función.
                if (extraStackArguments > 0)
                {
                    emitter.emitAddEspImm32(
                        static_cast<std::int32_t>(
                            extraStackArguments * 4));
                }

                // ------------------------------------------------
                // Resultado de la función:
                //
                //     EAX -> %N
                // ------------------------------------------------

                bindTemporaryToEax(
                    instruction.result);
            }


            // ------------------------------------------------
            // RETURN
            // ------------------------------------------------

            else if (instruction.opcode ==
                     IROpcode::Return)
            {
                /*
                 * RETURN
                 *
                 * Hay dos formas:
                 *
                 *     RETURN 42
                 *
                 * o:
                 *
                 *     RETURN %0
                 *
                 * En el segundo caso el temporal ya
                 * contiene el resultado en EAX.
                 */

                if (!instruction.operand.empty())
                {
                    /*
                     * RETURN %0
                     *
                     * El temporal ya está en EAX.
                     *
                     * No hacemos MOV EAX, inmediato,
                     * porque destruiríamos el resultado.
                     */

                    const auto temporaryIt =
                        temporaries.find(
                            instruction.operand);

                    if (temporaryIt ==
                        temporaries.end())
                    {
                        throw std::runtime_error(
                            "K++ X86Backend: RETURN de temporal no encontrado: " +
                            instruction.operand);
                    }

                    if (temporaryIt->second.kind !=
                        TemporaryLocation::Kind::Register)
                    {
                        throw std::runtime_error(
                            "K++ X86Backend: RETURN de temporal sin registro: " +
                            instruction.operand);
                    }

                    /*
                     * RETURN debe entregar el resultado en EAX.
                     *
                     * El allocator puede haber colocado el temporal
                     * en EAX o EBX.
                     *
                     * EAX:
                     *     el valor ya está donde RETURN lo necesita.
                     *
                     * EBX:
                     *     trasladamos el resultado a EAX.
                     */
                    if (temporaryIt->second.registerCode == 3)
                    {
                        emitter.emitMovEaxEbx();
                    }
                    else if (temporaryIt->second.registerCode != 0)
                    {
                        throw std::runtime_error(
                            "K++ X86Backend: RETURN usa registro no soportado: " +
                            instruction.operand);
                    }
                }
                else
                {
                    /*
                     * RETURN inmediato.
                     *
                     * Ejemplo:
                     *
                     *     return 42;
                     *
                     * EAX = 42
                     */

                    emitter.emitMovEaxImm32(
                        instruction.value);
                }

                // ------------------------------------------------
                // RETURN DE FUNCIÓN
                //
                // Las funciones llamadas devuelven mediante RET.
                //
                // main mantiene el mecanismo especial de
                // KolibriOS: salto al event loop.
                // ------------------------------------------------

                if (currentFunctionName != "main")
                {
                    if (frameActive)
                    {
                        emitter.emitMovEspEbp();
                        emitter.emitPopEbp();
                        emitter.emitRet();
                    }
                    else
                    {
                        emitter.emitRet();
                    }
                }
                else
                {
                    // main también debe abandonar su marco EBP.
                    if (frameActive)
                    {
                        emitter.emitMovEspEbp();
                        emitter.emitPopEbp();
                    }

                    emitter.emitByte(0xE9);

                    returnEventLoopPatches.push_back(
                        emitter.emitRel32Placeholder());
                }
            }
        }

        // Fin del cuerpo de esta IRFunction.
        finishCurrentFunctionFrame();
    }


    // ========================================================
    // RETURN IMPLÍCITO
    // ========================================================

    if (returnEventLoopPatches.empty())
    {
        /*
         * Si por alguna razón el programa no contiene
         * RETURN, K++ genera:
         *
         * return 0;
         */

        emitter.emitMovEaxImm32(0);

        emitter.emitByte(0xE9);

        returnEventLoopPatches.push_back(
            emitter.emitRel32Placeholder());
    }


    // ========================================================
    // PATCH MAIN
    // ========================================================

    emitter.patchRel32(
        runtimeResult.mainEntryPatch,
        mainBody);

    // REDRAW ya no entra a MAIN_BODY: el runtime lo
    // devuelve el solo al EVENT_LOOP y DRAW_WINDOW
    // repinta el buffer de lineas (parche del writer).


    // ========================================================
    // PATCH RETURN
    // ========================================================

    for (const auto patchOffset :
         returnEventLoopPatches)
    {
        emitter.patchRel32(
            patchOffset,
            runtimeResult.eventLoopAddress);
    }


    // --------------------------------------------------------
    // Guardar el contexto de variables de la última función.
    //
    // FUNCTION_END no necesita lógica especial aquí porque
    // el backend genera todas las funciones secuencialmente.
    // --------------------------------------------------------

    if (!currentFunctionName.empty())
    {
        functionVariables[
            currentFunctionName] =
            variables;
    }


    // ========================================================
    // PATCH CONTROL FLOW
    // ========================================================

    /*
     * JUMP_IF_ZERO ya generó:
     *
     *     CMP EAX, 0
     *     JE  <rel32>
     *
     * El rel32 quedó pendiente porque en el momento
     * de generar el salto todavía no necesariamente
     * conocíamos la posición final de la etiqueta.
     *
     * Ahora todas las instrucciones fueron generadas
     * y las etiquetas contienen su posición real.
     */

    for (const auto& patch :
         controlFlowPatches)
    {
        const auto labelIt =
            labels.find(patch.label);

        if (labelIt == labels.end())
        {
            throw std::runtime_error(
                "K++ X86Backend: etiqueta no definida: " +
                patch.label);
        }

        emitter.patchRel32(
            patch.patchOffset,
            labelIt->second);
    }


    // ========================================================
    // PATCH FUNCTION CALLS
    // ========================================================
    //
    // Cada CALL fue generado como:
    //
    //     E8 00 00 00 00
    //
    // Ahora conocemos la dirección de todas las funciones.
    // ========================================================

    for (const auto& patch :
         functionCallPatches)
    {
        const auto functionIt =
            functionEntries.find(
                patch.functionName);

        if (functionIt ==
            functionEntries.end())
        {
            throw std::runtime_error(
                "K++ X86Backend: función no definida: " +
                patch.functionName);
        }

        emitter.patchRel32(
            patch.patchOffset,
            functionIt->second);
    }


    // ========================================================
    // STRING POOL
    // ========================================================

    const std::size_t codeSize =
        emitter.size();


// ========================================================
// INTEGER PRINT BUFFER
// ========================================================

if (!integerBufferPatches.empty())
{
    integerBufferOffset =
        data.size();

    // 15 caracteres + terminador NUL.
    data.resize(
        data.size() + 16,
        0);
}


// ========================================================
// CURSOR GRÁFICO (DATA) — KPP_CURSOR_DATA
// ========================================================

cursorDataOffset =
    data.size();

// X/Y empaquetado: 4 bytes.
data.resize(
    data.size() + 4,
    0);


// ========================================================
// BUFFER DE LÍNEAS DEL WRITER (DATA)
//
// lineCount: cuántas líneas van anotadas (4 bytes).
// repaintCursor: X/Y empaquetado del repintado
// (4 bytes). lineBuffer: 128 ranuras de 64 bytes —
// [longitud de dibujo (4)][40 bytes de texto].
// Todo nace en cero: sin líneas, la ventana repinta
// vacía.
// ========================================================

lineHeadDataOffset =
    data.size();

data.resize(
    data.size() + 4,
    0);

lineCountDataOffset =
    data.size();

data.resize(
    data.size() + 4,
    0);

firstVisibleDataOffset =
    data.size();

data.resize(
    data.size() + 4,
    0);

repaintCursorDataOffset =
    data.size();

data.resize(
    data.size() + 4,
    0);

lineBufferDataOffset =
    data.size();

data.resize(
    data.size() + 32768,
    0);


// ========================================================
// BARRA DE LA KONSOLE (DATA) — rebanada 4
//
// thumbTop / thumbHeight: la geometría del pulgar
// que dibuja la barra y lee el ratón. dragState:
// 1 mientras el pulgar está agarrado. dragGrab: el
// desplazamiento del puntero dentro del pulgar al
// agarrarlo. Todo nace en cero.
// ========================================================

thumbTopDataOffset =
    data.size();

data.resize(
    data.size() + 4,
    0);

thumbHeightDataOffset =
    data.size();

data.resize(
    data.size() + 4,
    0);

dragStateDataOffset =
    data.size();

data.resize(
    data.size() + 4,
    0);

dragGrabDataOffset =
    data.size();

data.resize(
    data.size() + 4,
    0);



// ========================================================
// STRING POOL
// ========================================================

std::vector<std::size_t>
    stringOffsets;


    for (const auto& text :
         strings)
    {
        stringOffsets.push_back(
            data.size());

        for (const char c :
             text)
        {
            data.push_back(
                static_cast<std::uint8_t>(
                    c));
        }

        // terminador C
        data.push_back(0);
    }

    // ========================================================
    // STRING CONSTANTS
    // ========================================================

    std::vector<std::size_t>
        stringConstOffsets;

    for (const auto& text :
        stringConstTexts)
    {
        stringConstOffsets.push_back(
            data.size());

        for (const char c :
            text)
        {
            data.push_back(
                static_cast<std::uint8_t>(
                    c));
        }

        // terminador C
        data.push_back(0);
    }

    // ========================================================
    // KPP_FILE_IO_RUNTIME_BACKEND_PHASE2
    // SCRATCH Y BLOQUE DE PARÁMETROS
    // ========================================================

    std::size_t fileIoScratchOffset = 0;
    std::size_t fileIoParamsOffset = 0;

    if (needsFileIoRuntime)
    {
        // Scratch compartido de transferencia.
        fileIoScratchOffset = data.size();

        data.resize(
            data.size() + KOLIBRI_FILE_IO_MAX_BYTES,
            0);

        // Alinear el comienzo del bloque a cuatro bytes.
        while ((data.size() % 4u) != 0u)
        {
            data.push_back(0);
        }

        fileIoParamsOffset = data.size();

        // La estructura empaquetada ocupa exactamente 25 bytes.
        data.resize(
            data.size() + sizeof(KolibriFileIoParams),
            0);
    }


    // ========================================================
    // DATA BASE
    // ========================================================

    /*
     * Dirección absoluta dentro del BIN:
     *
     * HEADER + CODE + DATA
     */

    const std::uint32_t
        dataBase =
            KOLIBRI_HEADER_SIZE +
            static_cast<std::uint32_t>(
                codeSize);


    // ========================================================
    // KPP_FILE_IO_RUNTIME_BACKEND_PHASE2
    // PATCH DIRECCIONES DEL RUNTIME FILE I/O
    // ========================================================

    if (needsFileIoRuntime)
    {
        const std::uint32_t scratchAddress =
            dataBase +
            static_cast<std::uint32_t>(
                fileIoScratchOffset);

        const std::uint32_t paramsAddress =
            dataBase +
            static_cast<std::uint32_t>(
                fileIoParamsOffset);

        // FILE_WRITE.
        emitter.patch32(
            fileIoRuntimeResult.writeScratchAddressPatch,
            scratchAddress);

        emitter.patch32(
            fileIoRuntimeResult.writeParamsAddressPatch,
            paramsAddress);

        // FILE_READ.
        emitter.patch32(
            fileIoRuntimeResult.readScratchAddressPatch,
            scratchAddress);

        emitter.patch32(
            fileIoRuntimeResult.readParamsAddressPatch,
            paramsAddress);
    }


    // ========================================================
    // PATCH STRING CONSTANTS
    // ========================================================

    for (const auto& patch :
         stringConstPatches)
    {
        if (patch.stringIndex >=
            stringConstOffsets.size())
        {
            throw std::runtime_error(
                "K++ X86Backend: índice de STRING_CONST fuera de rango.");
        }

        const std::uint32_t
            stringAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    stringConstOffsets[
                        patch.stringIndex]);

        emitter.patch32(
            patch.patchOffset,
            stringAddress);
    }


    // ========================================================
    // ========================================================
    // KPP_FILE_IO_ADDRESS_RESOLUTION_PHASE5
    //
    // RESOLUCIÓN DE DIRECCIONES FILE_WRITE / FILE_READ
    //
    // En este punto:
    //
    //     - dataBase ya está calculado.
    //     - DATA ya contiene las rutas.
    //     - Los contextos functionVariables están completos.
    //     - Los placeholders imm32 siguen pendientes.
    //
    // Las direcciones de los arrays se recuperan desde
    // el contexto correspondiente a cada función.
    //
    // No se utiliza el mapa temporal "variables" porque
    // en este punto puede pertenecer a la última función.
    // ========================================================

    const auto resolveFileIoPatch =
        [&](const auto& patch, const char* operation)
        {
            // ------------------------------------------------
            // VALIDAR CONTEXTO DE FUNCIÓN
            // ------------------------------------------------

            if (patch.functionName.empty())
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": functionName vacío.");
            }

            const auto functionIt =
                functionVariables.find(
                    patch.functionName);

            if (functionIt ==
                functionVariables.end())
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": contexto de variables no encontrado: "
                    + patch.functionName);
            }

            // ------------------------------------------------
            // LOCALIZAR ARRAY EN EL CONTEXTO DE SU FUNCIÓN
            // ------------------------------------------------

            if (patch.arrayName.empty())
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": arrayName vacío.");
            }

            const auto arrayIt =
                functionIt->second.find(
                    patch.arrayName);

            if (arrayIt ==
                functionIt->second.end())
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": array no encontrado: "
                    + patch.functionName
                    + "::"
                    + patch.arrayName);
            }

            const VariableLocation& arrayLocation =
                arrayIt->second;

            // ------------------------------------------------
            // VALIDAR QUE SEA UN ARRAY EN DATA
            // ------------------------------------------------

            if (arrayLocation.isStackLocal)
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": el array está ubicado en stack: "
                    + patch.functionName
                    + "::"
                    + patch.arrayName);
            }

            if (arrayLocation.arrayCapacity == 0)
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": capacidad del array no registrada: "
                    + patch.arrayName);
            }

            if (patch.capacity == 0 ||
                patch.capacity !=
                    arrayLocation.arrayCapacity)
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": capacidad del parche incompatible "
                    "con el array: "
                    + patch.arrayName);
            }

            // ------------------------------------------------
            // VALIDAR OFFSET DEL ARRAY DENTRO DE DATA
            // ------------------------------------------------

            const std::size_t arrayDataOffset =
                arrayLocation.dataOffset;

            if (arrayDataOffset >= data.size())
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": offset del array fuera de DATA: "
                    + patch.arrayName);
            }

            // Cada celda del array ocupa un uint32_t.
            //
            // Se evita multiplicar antes de comprobar
            // los límites para prevenir desbordamientos.

            const std::size_t remainingData =
                data.size() - arrayDataOffset;

            if (arrayLocation.arrayCapacity >
                remainingData / sizeof(std::uint32_t))
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": el array completo excede DATA: "
                    + patch.arrayName);
            }

            // ------------------------------------------------
            // VALIDAR OFFSET DE LA RUTA DENTRO DE DATA
            // ------------------------------------------------

            if (patch.pathDataOffset >= data.size())
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": pathDataOffset fuera de DATA.");
            }

            // ------------------------------------------------
            // VALIDAR OFFSETS IMM32 DENTRO DEL CÓDIGO
            // ------------------------------------------------

            const std::size_t codeSize =
                emitter.size();

            const auto validateCodePatch =
                [&](std::size_t offset,
                    const char* field)
                {
                    if (offset > codeSize ||
                        codeSize - offset < 4)
                    {
                        throw std::runtime_error(
                            std::string(
                                "K++ X86Backend: ")
                            + operation
                            + ": "
                            + field
                            + " fuera del buffer de código.");
                    }
                };

            validateCodePatch(
                patch.pathAddressPatch,
                "pathAddressPatch");

            validateCodePatch(
                patch.arrayAddressPatch,
                "arrayAddressPatch");

            if (patch.pathAddressPatch ==
                patch.arrayAddressPatch)
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": pathAddressPatch y "
                    "arrayAddressPatch coinciden.");
            }

            // ------------------------------------------------
            // VALIDAR PLACEHOLDERS ORIGINALES
            //
            // Ambos imm32 deben seguir a cero.
            // Esto evita parchear accidentalmente otra
            // instrucción o resolver dos veces el mismo sitio.
            // ------------------------------------------------

            const auto& machineCode =
                emitter.code();

            const auto validateZeroPlaceholder =
                [&](std::size_t offset,
                    const char* field)
                {
                    for (std::size_t i = 0; i < 4; ++i)
                    {
                        if (machineCode[offset + i] != 0)
                        {
                            throw std::runtime_error(
                                std::string(
                                    "K++ X86Backend: ")
                                + operation
                                + ": placeholder "
                                + field
                                + " ya no está a cero.");
                        }
                    }
                };

            validateZeroPlaceholder(
                patch.pathAddressPatch,
                "pathAddressPatch");

            validateZeroPlaceholder(
                patch.arrayAddressPatch,
                "arrayAddressPatch");

            // ------------------------------------------------
            // CALCULAR DIRECCIONES ABSOLUTAS
            //
            // Dirección de ruta:
            //
            //     dataBase + pathDataOffset
            //
            // Dirección de array:
            //
            //     dataBase + arrayLocation.dataOffset
            // ------------------------------------------------

            const std::uint64_t pathAddress64 =
                static_cast<std::uint64_t>(dataBase)
                + static_cast<std::uint64_t>(
                    patch.pathDataOffset);

            const std::uint64_t arrayAddress64 =
                static_cast<std::uint64_t>(dataBase)
                + static_cast<std::uint64_t>(
                    arrayDataOffset);

            // ------------------------------------------------
            // VALIDAR RANGO DE DIRECCIONES X86
            // ------------------------------------------------

            constexpr std::uint64_t MAX_UINT32 =
                0xFFFFFFFFull;

            if (pathAddress64 > MAX_UINT32)
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": dirección absoluta de ruta "
                    "excede uint32.");
            }

            if (arrayAddress64 > MAX_UINT32)
            {
                throw std::runtime_error(
                    std::string(
                        "K++ X86Backend: ")
                    + operation
                    + ": dirección absoluta del array "
                    "excede uint32.");
            }

            const std::uint32_t pathAddress =
                static_cast<std::uint32_t>(
                    pathAddress64);

            const std::uint32_t arrayAddress =
                static_cast<std::uint32_t>(
                    arrayAddress64);

            // ------------------------------------------------
            // PATCH IMM32 LITTLE-ENDIAN
            //
            // X86Emitter::patch32 escribe:
            //
            //     byte 0 = value & 0xFF
            //     byte 1 = value >> 8
            //     byte 2 = value >> 16
            //     byte 3 = value >> 24
            //
            // No se implementa otro mecanismo de parcheo.
            // ------------------------------------------------

            emitter.patch32(
                patch.pathAddressPatch,
                pathAddress);

            emitter.patch32(
                patch.arrayAddressPatch,
                arrayAddress);
        };


    // ========================================================
    // PATCH FILE_WRITE
    // ========================================================

    for (const auto& patch :
         fileWritePatches)
    {
        resolveFileIoPatch(
            patch,
            "FILE_WRITE");
    }


    // ========================================================
    // PATCH FILE_READ
    // ========================================================

    for (const auto& patch :
         fileReadPatches)
    {
        resolveFileIoPatch(
            patch,
            "FILE_READ");
    }


    // PATCH TEMPORARY SPILLS
    // ========================================================

    /*
     * Cada spill generó previamente una instrucción
     * MOV [00000000], EAX
     * o
     * MOV [00000000], EBX
     *
     * Ahora resolvemos la dirección absoluta:
     *
     *     dataBase + temporary.dataOffset
     */

    for (const auto& patch :
        temporarySpillPatches)
    {
        // El valor es un displacement firmado respecto a EBP.
        // La conversión a uint32_t conserva sus bits en complemento
        // a dos para escribir el disp32 de la instrucción x86.
        emitter.patch32(
            patch.patchOffset,
            static_cast<std::uint32_t>(patch.dataOffset));
    }


    // ========================================================
    // PATCH TEMPORARY RELOADS
    // ========================================================

    /*
     * Cada reload lee el temporal desde su slot EBP-relative.
     * Aquí se escribe el displacement negativo del marco.
     */

    for (const auto& patch :
        temporaryReloadPatches)
    {
        // El reload lee desde [EBP + displacement].
        emitter.patch32(
            patch.patchOffset,
            static_cast<std::uint32_t>(patch.dataOffset));
    }


    // ========================================================
    // PATCH LOAD ADDRESSES
    // ========================================================

    /*
     * Cada LOAD generó previamente:
     *
     *     MOV EAX, [00000000]
     *
     * Ahora que conocemos dataBase podemos resolver
     * la dirección absoluta de cada variable.
     *
     * Dirección:
     *
     *     dataBase + variable.dataOffset
     */

    for (const auto& patch :
         variableLoadPatches)
    {
        const auto functionIt =
            functionVariables.find(
                patch.functionName);

        if (functionIt ==
            functionVariables.end())
        {
            throw std::runtime_error(
                "K++ X86Backend: contexto de variables LOAD no encontrado para función: " +
                patch.functionName);
        }

        const auto variableIt =
            functionIt->second.find(
                patch.name);

        if (variableIt ==
            functionIt->second.end())
        {
            throw std::runtime_error(
                "K++ X86Backend: variable LOAD no encontrada durante patch: " +
                patch.functionName +
                "::" +
                patch.name);
        }

        const std::uint32_t
            variableAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    variableIt->second.dataOffset);

        emitter.patch32(
            patch.patchOffset,
            variableAddress);
    }


    // ========================================================
    // ========================================================
// PATCH STORE ADDRESSES
// ========================================================

for (const auto& patch :
     variableStorePatches)
{
    const auto functionIt =
        functionVariables.find(
            patch.functionName);

    if (functionIt ==
        functionVariables.end())
    {
        throw std::runtime_error(
            "K++ X86Backend: contexto de variables STORE no encontrado para función: " +
            patch.functionName);
    }

    const auto variableIt =
        functionIt->second.find(
            patch.name);

    if (variableIt ==
        functionIt->second.end())
    {
        throw std::runtime_error(
            "K++ X86Backend: variable STORE no encontrada durante patch: " +
            patch.functionName +
            "::" +
            patch.name);
    }

    const std::uint32_t
        variableAddress =
            dataBase +
            static_cast<std::uint32_t>(
                variableIt->second.dataOffset);

    emitter.patch32(
        patch.patchOffset,
        variableAddress);
}

// TITLE ADDRESS
    // ========================================================

    const std::uint32_t
        titleAddress =
            dataBase +
            static_cast<std::uint32_t>(
                stringOffsets[0]);


    emitter.patch32(
        runtimeResult.titleAddressPatch,
        titleAddress);

    emitter.patch32(
        runtimeResult.titleCaptionPatch,
        titleAddress);






    // ========================================================
    // PATCH INTEGER PRINT BUFFER
    // ========================================================

    if (!integerBufferPatches.empty())
    {
        // El runtime espera EDI apuntando al FINAL
        // del buffer: escribe el NUL en EDI-1 y los
        // digitos hacia atras. La reserva es de 16
        // bytes (15 digitos + NUL), asi que el imm32
        // se parchea con base + 16, no con la base.
        const std::uint32_t
            integerBufferAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    integerBufferOffset) +
                16;

        for (const std::size_t patch :
             integerBufferPatches)
        {
            emitter.patch32(
                patch,
                integerBufferAddress);
        }
    }


    // ========================================================
    // PATCH CURSOR GRÁFICO — KPP_CURSOR_DATA
    // ========================================================

    {
        const std::uint32_t
            cursorAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    cursorDataOffset);

        for (const std::size_t patch :
             runtime.cursorPatches())
        {
            emitter.patch32(
                patch,
                cursorAddress);
        }
    }


    // ========================================================
    // PATCH BUFFER DE LÍNEAS — el contador, el cursor
    // de repintado y la base de las ranuras.
    // ========================================================

    {
        const std::uint32_t
            lineCountAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    lineCountDataOffset);

        for (const std::size_t patch :
             runtime.lineCountPatches())
        {
            emitter.patch32(
                patch,
                lineCountAddress);
        }

        const std::uint32_t
            lineHeadAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    lineHeadDataOffset);

        for (const std::size_t patch :
             runtime.lineHeadPatches())
        {
            emitter.patch32(
                patch,
                lineHeadAddress);
        }

        const std::uint32_t
            firstVisibleAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    firstVisibleDataOffset);

        for (const std::size_t patch :
             runtime.firstVisiblePatches())
        {
            emitter.patch32(
                patch,
                firstVisibleAddress);
        }

        const std::uint32_t
            thumbTopAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    thumbTopDataOffset);

        for (const std::size_t patch :
             runtime.thumbTopPatches())
        {
            emitter.patch32(
                patch,
                thumbTopAddress);
        }

        const std::uint32_t
            thumbHeightAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    thumbHeightDataOffset);

        for (const std::size_t patch :
             runtime.thumbHeightPatches())
        {
            emitter.patch32(
                patch,
                thumbHeightAddress);
        }

        const std::uint32_t
            dragStateAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    dragStateDataOffset);

        for (const std::size_t patch :
             runtime.dragStatePatches())
        {
            emitter.patch32(
                patch,
                dragStateAddress);
        }

        const std::uint32_t
            dragGrabAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    dragGrabDataOffset);

        for (const std::size_t patch :
             runtime.dragGrabPatches())
        {
            emitter.patch32(
                patch,
                dragGrabAddress);
        }

        const std::uint32_t
            repaintCursorAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    repaintCursorDataOffset);

        for (const std::size_t patch :
             runtime.repaintCursorPatches())
        {
            emitter.patch32(
                patch,
                repaintCursorAddress);
        }

        const std::uint32_t
            lineBufferAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    lineBufferDataOffset);

        for (const std::size_t patch :
             runtime.lineBufferPatches())
        {
            emitter.patch32(
                patch,
                lineBufferAddress);
        }
    }


    // ========================================================
    // PATCH KONSOLE — las llamadas de los PRINT a
    // REFRESH_OUTPUT (rebanada 3): saltos relativos
    // a la rutina emitida con el runtime gráfico.
    // ========================================================

    {
        for (const std::size_t patch :
             runtime.refreshCallPatches())
        {
            emitter.patchRel32(
                patch,
                runtimeResult.refreshOutputAddress);
        }
    }


    // ========================================================
    // CODE + DATA
    // ========================================================

    std::vector<std::uint8_t>
        result =
            emitter.code();

    result.insert(
        result.end(),
        data.begin(),
        data.end());


    return result;
}



// ============================================================
// GENERATE FUNCTION
// ============================================================

void
X86Backend::generateFunction(
    const IRFunction& function)
{
    /*
     * El backend principal genera actualmente
     * el cuerpo directamente desde IRProgram.
     *
     * Esta función se conserva para mantener
     * la arquitectura preparada para futuras
     * funciones C++.
     */

    (void)function;
}

}
