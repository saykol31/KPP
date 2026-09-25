

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
                IROpcode::Print)
            {
                strings.push_back(
                    instruction.name);
            }
        }
    }


    // --------------------------------------------------------
    // Cantidad de PRINT
    // --------------------------------------------------------

    const std::size_t printCount =
        strings.size() > 1
            ? strings.size() - 1
            : 0;


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

    const std::size_t mainBody =
        emitter.size();


    std::vector<std::size_t>
        textPatches;

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

    resetTemporaries();
    resetVariables();
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
            // MUL
            // ------------------------------------------------

            else if (instruction.opcode ==
                    IROpcode::Mul)
            {
                /*
                * MUL %1, %2 -> %3
                *
                * En esta primera versión del backend:
                *
                *     izquierda = EAX
                *     derecha   = EBX
                *
                * Generamos:
                *
                *     IMUL EAX, EBX
                *
                * Resultado:
                *
                *     EAX = EAX * EBX
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
                        "K++ X86Backend: MUL usa temporal izquierdo inexistente: " +
                        left);
                }


                if (!hasTemporary(right))
                {
                    throw std::runtime_error(
                        "K++ X86Backend: MUL usa temporal derecho inexistente: " +
                        right);
                }


                const std::uint8_t leftRegister =
                    getTemporaryRegister(left);

                const std::uint8_t rightRegister =
                    getTemporaryRegister(right);


                if (leftRegister == 0 &&
                    rightRegister == 3)
                {
                    /*
                     * Caso normal:
                     *
                     *     izquierda = EAX
                     *     derecha   = EBX
                     */

                    emitter.emitMulEaxEbx();
                }
                else if (leftRegister == 3 &&
                         rightRegister == 0)
                {
                    /*
                     * Tenemos:
                     *
                     *     izquierda = EBX
                     *     derecha   = EAX
                     *
                     * Queremos:
                     *
                     *     izquierda = EAX
                     *     derecha   = EBX
                     *
                     * Transformación:
                     *
                     *     PUSH EAX
                     *     MOV  EAX, EBX
                     *     POP  EBX
                     */

                    emitter.emitPushEax();
                    emitter.emitMovEaxEbx();
                    emitter.emitPopEbx();

                    emitter.emitMulEaxEbx();
                }
                else
                {
                    throw std::runtime_error(
                        "K++ X86Backend: MUL no puede preparar los operandos.");
                }


                /*
                * El resultado de MUL queda en EAX.
                */

                bindTemporaryToEax(
                    instruction.result);

                // ========================================================
                // DIAGNOSTICO TEMPORALES DESPUES DE MUL
                // ========================================================

                std::cerr << "[TEMP STATE AFTER MUL]" << '\n';

                for (const auto& entry : temporaries)
                {
                    const auto& name = entry.first;
                    const auto& location = entry.second;

                    std::cerr << "    "
                              << name
                              << " -> ";

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
                        std::cerr << "MEM["
                                  << location.dataOffset
                                  << "]";
                    }

                    std::cerr << '\n';
                }


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
                    instruction.name,
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
                    instruction.name,
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

                if (labels.find(instruction.name) !=
                    labels.end())
                {
                    throw std::runtime_error(
                        "K++ X86Backend: LABEL duplicado: " +
                        instruction.name);
                }

                labels[instruction.name] =
                    emitter.size();
            }


            // ------------------------------------------------
            // PRINT
            // ------------------------------------------------

            else if (instruction.opcode ==
                     IROpcode::Print)
            {
                /*
                 * PRINT 1 -> Y=40
                 * PRINT 2 -> Y=70
                 * PRINT 3 -> Y=100
                 * PRINT 4 -> Y=130
                 */

                const std::uint32_t y =
                    40u +
                    static_cast<std::uint32_t>(
                        printIndex * 20u);


                const std::size_t patch =
                    runtime.emitPrint(
                        emitter,
                        y);


                textPatches.push_back(
                    patch);

                ++printIndex;
            }


            // ------------------------------------------------
            // PRINT INTEGER
            // ------------------------------------------------

            else if (instruction.opcode ==
                     IROpcode::PrintInteger)
            {
                if (!hasTemporary(
                        instruction.operand))
                {
                    throw std::runtime_error(
                        "PRINT_INT usa un temporal inexistente: " +
                        instruction.operand);
                }

                const std::uint8_t reg =
                    getTemporaryRegister(
                        instruction.operand);

                /*
                 * PRINT_INT utiliza EAX como registro de entrada.
                 *
                 * El allocator puede haber colocado el temporal
                 * en EAX o EBX.
                 *
                 * EAX:
                 *     no hacemos nada.
                 *
                 * EBX:
                 *     MOV EAX, EBX
                 *
                 * De esta forma PRINT_INT siempre recibe el valor
                 * correcto en EAX sin imponer al allocator que
                 * todos los resultados finales vivan en EAX.
                 */
                if (reg == 3)
                {
                    emitter.emitMovEaxEbx();
                }
                else if (reg != 0)
                {
                    throw std::runtime_error(
                        "PRINT_INT: registro temporal no soportado.");
                }

                const std::uint32_t y =
                    40u +
                    static_cast<std::uint32_t>(
                        printIndex * 20u);

                const std::size_t patch =
                    runtime.emitPrintInteger(
                        emitter,
                        y);

                integerBufferPatches.push_back(
                    patch);

                ++printIndex;
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

                emitter.emitByte(0xE9);

                returnEventLoopPatches.push_back(
                    emitter.emitRel32Placeholder());
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
        const auto variableIt =
            variables.find(patch.name);

        if (variableIt == variables.end())
        {
            throw std::runtime_error(
                "K++ X86Backend: variable LOAD no encontrada durante patch: " +
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
    const auto variableIt =
        variables.find(patch.name);

    if (variableIt == variables.end())
    {
        throw std::runtime_error(
            "K++ X86Backend: variable STORE no encontrada durante patch: " +
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
    // VALIDAR PRINT PATCHES
    // ========================================================

    if (textPatches.size()
        != printCount)
    {
        throw std::runtime_error(
            "K++: cantidad de patches PRINT incorrecta");
    }


    // ========================================================
    // PATCH PRINT ADDRESSES
    // ========================================================

    for (std::size_t i = 0;
         i < printCount;
         ++i)
    {
        /*
         * strings:
         *
         * [0] = title
         * [1] = PRINT 1
         * [2] = PRINT 2
         * [3] = PRINT 3
         */

        const std::uint32_t
            textAddress =
                dataBase +
                static_cast<std::uint32_t>(
                    stringOffsets[i + 1]);


        emitter.patch32(
            textPatches[i],
            textAddress);
    }


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
