

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


    std::size_t
    X86Backend::allocateTemporarySpill(
        std::vector<std::uint8_t>& data)
    {
        const std::size_t offset =
            data.size();

        data.resize(
            data.size() + 4,
            0);

        return offset;
    }


    void X86Backend::bindTemporaryToMemory(
        const std::string& temporary,
        std::size_t dataOffset)
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

        const std::size_t dataOffset =
            allocateTemporarySpill(data);

        std::size_t patchOffset = 0;

        if (registerCode == 0)
        {
            /*
            * MOV [direccion], EAX
            */
            patchOffset =
                emitter.emitMovMemEax32Placeholder();
        }
        else if (registerCode == 3)
        {
            /*
            * MOV [direccion], EBX
            */
            patchOffset =
                emitter.emitMovMemEbx32Placeholder();
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

        const std::size_t dataOffset =
            it->second.dataOffset;

        /*
         * Generamos:
         *
         *     MOV EAX, [00000000]
         *
         * La dirección absoluta se resolverá
         * posteriormente mediante dataBase.
         */

        const std::size_t patchOffset =
            emitter.emitMovEaxMem32Placeholder();

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
    }

    std::size_t
    X86Backend::allocateVariableData(
        std::vector<std::uint8_t>& data)
    {
        const std::size_t offset =
            data.size();

        data.resize(
            data.size() + 4,
            0);

        return offset;
    }

    void X86Backend::declareVariable(
        const std::string& name,
        std::size_t dataOffset)
    {
        variables[name] = {
            dataOffset
        };
    }

    bool X86Backend::hasTemporary(
        const std::string& temporary) const
    {
        return temporaries.find(temporary)
            != temporaries.end();
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

    std::size_t integerBufferOffset = 0;

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
    // FUNCIÓN ACTUAL
    // ========================================================

    std::string currentFunctionName;

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

                if (instruction.name == "main")
                {
                    mainBody =
                        emitter.size();
                }

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
                const std::size_t offset =
                    allocateVariableData(data);

                declareVariable(
                    instruction.name,
                    offset);
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

                std::size_t patchOffset = 0;

                if (reg == 0)
                {
                    /*
                     * Temporal en EAX.
                     *
                     *     MOV [moffs32], EAX
                     */
                    patchOffset =
                        emitter.emitMovMemEax32Placeholder();
                }
                else if (reg == 3)
                {
                    /*
                     * Temporal en EBX.
                     *
                     *     MOV [moffs32], EBX
                     */
                    patchOffset =
                        emitter.emitMovMemEbx32Placeholder();
                }
                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: STORE usa un registro temporal no soportado.");
                }

                variableStorePatches.push_back({
                    currentFunctionName,
                    instruction.name,
                    patchOffset
                });

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
            // LOAD
            // ------------------------------------------------

            // ------------------------------------------------
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

                if (registerCode == 0)
                {
                    /*
                     * MOV EAX, [direccion_de_x]
                     */

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
                    /*
                     * MOV EBX, [direccion_de_x]
                     */

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
                        emitter.emitMovEaxMem32Placeholder();

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
                        emitter.emitMovEaxMem32Placeholder();

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
                        emitter.emitMovEaxMem32Placeholder();

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
                      emitter.emitMovEaxMem32Placeholder();

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
                        emitter.emitMovEaxMem32Placeholder();

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
                        emitter.emitMovEaxMem32Placeholder();

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

else if (instruction.opcode ==
                     IROpcode::Sub)
            {
                const std::size_t comma =
                    instruction.operand.find(",");

                if (comma == std::string::npos)
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB requiere dos operandos");
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
                        "K++ X86Backend: SUB usa temporal izquierdo inexistente: " +
                        left);
                }

                if (!hasTemporary(right))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB usa temporal derecho inexistente: " +
                        right);
                }

                const auto leftIt =
                    temporaries.find(left);

                const auto rightIt =
                    temporaries.find(right);

                if (leftIt == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB usa temporal izquierdo inexistente: " +
                        left);
                }

                if (rightIt == temporaries.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: SUB usa temporal derecho inexistente: " +
                        right);
                }

                const auto leftKind =
                    leftIt->second.kind;

                const auto rightKind =
                    rightIt->second.kind;

                // ------------------------------------------------
                // CASO 1
                //
                // EAX - EBX
                //
                // EAX = EAX - EBX
                // ------------------------------------------------

                if (
                    leftKind ==
                        TemporaryLocation::Kind::Register &&
                    leftIt->second.registerCode == 0 &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 3)
                {
                    emitter.emitSubEaxEbx();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                // ------------------------------------------------
                // CASO 2
                //
                // EBX - EAX
                //
                // PUSH EAX
                // MOV  EAX, EBX
                // POP  EBX
                // SUB  EAX, EBX
                // ------------------------------------------------

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

                    emitter.emitSubEaxEbx();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                // ------------------------------------------------
                // CASO 3
                //
                // MEMORY - EAX
                //
                // PUSH EAX
                // MOV  EAX, [MEMORY]
                // POP  EBX
                // SUB  EAX, EBX
                // ------------------------------------------------

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Memory &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 0)
                {
                    emitter.emitPushEax();

                    const std::size_t patchOffset =
                        emitter.emitMovEaxMem32Placeholder();

                    temporaryReloadPatches.push_back({
                        left,
                        patchOffset,
                        leftIt->second.dataOffset
                    });

                    emitter.emitPopEbx();

                    emitter.emitSubEaxEbx();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                // ------------------------------------------------
                // CASO 4
                //
                // EAX - MEMORY
                //
                // Guardamos EAX = izquierdo.
                //
                // PUSH EAX
                // MOV  EAX, [MEMORY]
                // POP  EBX
                //
                // Ahora:
                //     EBX = izquierdo
                //     EAX = derecho
                //
                // Reordenamos:
                //
                // PUSH EAX
                // MOV  EAX, EBX
                // POP  EBX
                //
                // Ahora:
                //     EAX = izquierdo
                //     EBX = derecho
                //
                // SUB EAX, EBX
                // ------------------------------------------------

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Register &&
                    leftIt->second.registerCode == 0 &&
                    rightKind ==
                        TemporaryLocation::Kind::Memory)
                {
                    emitter.emitPushEax();

                    const std::size_t patchOffset =
                        emitter.emitMovEaxMem32Placeholder();

                    temporaryReloadPatches.push_back({
                        right,
                        patchOffset,
                        rightIt->second.dataOffset
                    });

                    emitter.emitPopEbx();

                    emitter.emitPushEax();
                    emitter.emitMovEaxEbx();
                    emitter.emitPopEbx();

                    emitter.emitSubEaxEbx();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                // ------------------------------------------------
                // CASO 5
                //
                // MEMORY - EBX
                //
                // MOV EAX, EBX
                // PUSH EAX
                // MOV EAX, [MEMORY]
                // POP EBX
                // SUB EAX, EBX
                // ------------------------------------------------

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Memory &&
                    rightKind ==
                        TemporaryLocation::Kind::Register &&
                    rightIt->second.registerCode == 3)
                {
                    emitter.emitMovEaxEbx();
                    emitter.emitPushEax();

                    const std::size_t patchOffset =
                        emitter.emitMovEaxMem32Placeholder();

                    temporaryReloadPatches.push_back({
                        left,
                        patchOffset,
                        leftIt->second.dataOffset
                    });

                    emitter.emitPopEbx();

                    emitter.emitSubEaxEbx();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                // ------------------------------------------------
                // CASO 6
                //
                // EBX - MEMORY
                //
                // MOV EAX, EBX
                // PUSH EAX
                // MOV EAX, [MEMORY]
                // POP EBX
                //
                // Ahora:
                //     EBX = izquierdo
                //     EAX = derecho
                //
                // Reordenamos para SUB:
                //
                // EAX = izquierdo
                // EBX = derecho
                // ------------------------------------------------

                else if (
                    leftKind ==
                        TemporaryLocation::Kind::Register &&
                    leftIt->second.registerCode == 3 &&
                    rightKind ==
                        TemporaryLocation::Kind::Memory)
                {
                    emitter.emitMovEaxEbx();
                    emitter.emitPushEax();

                    const std::size_t patchOffset =
                        emitter.emitMovEaxMem32Placeholder();

                    temporaryReloadPatches.push_back({
                        right,
                        patchOffset,
                        rightIt->second.dataOffset
                    });

                    emitter.emitPopEbx();

                    emitter.emitPushEax();
                    emitter.emitMovEaxEbx();
                    emitter.emitPopEbx();

                    emitter.emitSubEaxEbx();

                    freeTemporary(left);
                    freeTemporary(right);

                    bindTemporaryToEax(
                        instruction.result);
                }

                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: combinación de operandos SUB no soportada");
                }
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
                    emitter.emitMovEaxMem32Placeholder();

                temporaryReloadPatches.push_back({
                    left, leftPatch, leftIt->second.dataOffset
                });

                const std::size_t rightPatch =
                    emitter.emitMovEbxMem32Placeholder();

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
                    emitter.emitMovEaxMem32Placeholder();

                temporaryReloadPatches.push_back({
                    left, leftPatch, leftIt->second.dataOffset
                });

                // EBX = divisor.
                const std::size_t rightPatch =
                    emitter.emitMovEbxMem32Placeholder();

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
                    emitter.emitMovEaxMem32Placeholder();

                temporaryReloadPatches.push_back({
                    left, leftPatch, leftIt->second.dataOffset
                });

                // EBX = divisor.
                const std::size_t rightPatch =
                    emitter.emitMovEbxMem32Placeholder();

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
                //
                // Los argumentos pueden encontrarse:
                //
                //     EAX
                //     EBX
                //     MEMORY
                //
                // Antes de CALL los normalizamos físicamente.
                // ------------------------------------------------

                std::string left;
                std::string right;

                if (!instruction.operand.empty())
                {
                    const std::size_t comma =
                        instruction.operand.find(',');

                    left =
                        instruction.operand.substr(
                            0,
                            comma);

                    if (comma != std::string::npos)
                    {
                        right =
                            instruction.operand.substr(
                                comma + 1);

                        if (right.find(',') !=
                            std::string::npos)
                        {
                            throw std::runtime_error(
                                "K++ X86Backend: CALL soporta como máximo 2 argumentos");
                        }
                    }

                    while (!left.empty() &&
                           left.front() == ' ')
                    {
                        left.erase(
                            left.begin());
                    }

                    while (!left.empty() &&
                           left.back() == ' ')
                    {
                        left.pop_back();
                    }

                    while (!right.empty() &&
                           right.front() == ' ')
                    {
                        right.erase(
                            right.begin());
                    }

                    while (!right.empty() &&
                           right.back() == ' ')
                    {
                        right.pop_back();
                    }

                    if (left.empty())
                    {
                        throw std::runtime_error(
                            "K++ X86Backend: CALL argumento vacío");
                    }

                    const auto leftIt =
                        temporaries.find(left);

                    if (leftIt == temporaries.end())
                    {
                        throw std::runtime_error(
                            "K++ X86Backend: CALL argumento no encontrado: " +
                            left);
                    }

                    if (!right.empty())
                    {
                        const auto rightIt =
                            temporaries.find(right);

                        if (rightIt == temporaries.end())
                        {
                            throw std::runtime_error(
                                "K++ X86Backend: CALL argumento no encontrado: " +
                                right);
                        }
                    }

                    auto emitLoadMemoryToEax =
                        [&](const std::string& temporary,
                            std::size_t dataOffset)
                    {
                        const std::size_t reloadPatchOffset =
                            emitter.emitMovEaxMem32Placeholder();

                        temporaryReloadPatches.push_back({
                            temporary,
                            reloadPatchOffset,
                            dataOffset
                        });
                    };

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
                    emitter.emitRet();
                }
                else
                {
                    emitter.emitByte(0xE9);

                    returnEventLoopPatches.push_back(
                        emitter.emitRel32Placeholder());
                }
            }
        }
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

    emitter.patchRel32(
        runtimeResult.redrawMainPatch,
        mainBody);


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
        const std::uint32_t
            temporaryAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    patch.dataOffset);

        emitter.patch32(
            patch.patchOffset,
            temporaryAddress);
    }


    // ========================================================
    // PATCH TEMPORARY RELOADS
    // ========================================================

    /*
     * Cada reload generó previamente:
     *
     *     MOV EAX, [00000000]
     *
     * Ahora resolvemos la dirección absoluta del
     * temporal almacenado en memoria.
     *
     * Dirección:
     *
     *     dataBase + temporary.dataOffset
     */

    for (const auto& patch :
        temporaryReloadPatches)
    {
        const std::uint32_t
            temporaryAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    patch.dataOffset);

        emitter.patch32(
            patch.patchOffset,
            temporaryAddress);
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






    // ========================================================
    // PATCH INTEGER PRINT BUFFER
    // ========================================================

    if (!integerBufferPatches.empty())
    {
        const std::uint32_t
            integerBufferAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    integerBufferOffset);

        for (const std::size_t patch :
             integerBufferPatches)
        {
            emitter.patch32(
                patch,
                integerBufferAddress);
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
