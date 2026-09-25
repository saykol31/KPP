#include "kpp/BinaryWriter.hpp"

#include <fstream>

namespace kpp {

bool BinaryWriter::write(
    const std::string& filename,
    const std::vector<std::uint8_t>& data)
{
    std::ofstream file(
        filename,
        std::ios::binary);

    if (!file)
        return false;

    if (!data.empty())
    {
        file.write(
            reinterpret_cast<const char*>(
                data.data()),
            static_cast<std::streamsize>(
                data.size()));
    }

    return file.good();
}

}
