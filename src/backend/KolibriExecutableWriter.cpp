#include "kpp/KolibriExecutableWriter.hpp"

#include <fstream>

namespace kpp {

// ============================================================
// WRITE32
// ============================================================

void KolibriExecutableWriter::write32(
    std::vector<std::uint8_t>& data,
    std::uint32_t value)
{
    data.push_back(
        static_cast<std::uint8_t>(
            value & 0xFF));

    data.push_back(
        static_cast<std::uint8_t>(
            (value >> 8) & 0xFF));

    data.push_back(
        static_cast<std::uint8_t>(
            (value >> 16) & 0xFF));

    data.push_back(
        static_cast<std::uint8_t>(
            (value >> 24) & 0xFF));
}


// ============================================================
// WRITE
// ============================================================

bool KolibriExecutableWriter::write(
    const std::string& filename,
    const std::vector<std::uint8_t>& code,
    std::uint32_t bssSize)
{
    /*
     * ========================================================
     * KOLIBRIOS HEADER
     * ========================================================
     *
     * 00  MENUET01
     * 08  version
     * 0C  start
     * 10  image end
     * 14  memory end
     * 18  stack
     * 1C  parameters
     * 20  path
     *
     * 36 bytes
     */

    const std::uint32_t
        start =
            HeaderSize;


    const std::uint32_t
        imageEnd =
            HeaderSize +
            static_cast<std::uint32_t>(
                code.size());


    // REBANADA BSS: entre imageEnd y memoryEnd vive la
    // zona BSS — datos que nacen en cero y no se
    // escriben en el archivo; el cargador la rellena
    // con ceros. La pila queda tras la BSS, como antes.
    const std::uint32_t
        stackTop =
            imageEnd +
            bssSize +
            StackSize;


    const std::uint32_t
        memoryEnd =
            stackTop;


    std::vector<std::uint8_t>
        executable;


    executable.reserve(
        HeaderSize +
        code.size());


    // ========================================================
    // SIGNATURE
    // ========================================================

    const char signature[] =
        "MENUET01";

    for (int i = 0; i < 8; ++i)
    {
        executable.push_back(
            static_cast<std::uint8_t>(
                signature[i]));
    }


    // ========================================================
    // VERSION
    // ========================================================

    write32(
        executable,
        1);


    // ========================================================
    // START
    // ========================================================

    write32(
        executable,
        start);


    // ========================================================
    // IMAGE END
    // ========================================================

    write32(
        executable,
        imageEnd);


    // ========================================================
    // MEMORY END
    // ========================================================

    write32(
        executable,
        memoryEnd);


    // ========================================================
    // STACK TOP
    // ========================================================

    write32(
        executable,
        stackTop);


    // ========================================================
    // PARAMETERS
    // ========================================================

    write32(
        executable,
        0);


    // ========================================================
    // PATH
    // ========================================================

    write32(
        executable,
        0);


    // ========================================================
    // CODE + DATA
    // ========================================================

    executable.insert(
        executable.end(),
        code.begin(),
        code.end());


    // ========================================================
    // WRITE
    // ========================================================

    std::ofstream file(
        filename,
        std::ios::binary);

    if (!file)
        return false;


    file.write(
        reinterpret_cast<
            const char*>(
                executable.data()),
        static_cast<
            std::streamsize>(
                executable.size()));


    return file.good();
}

}
