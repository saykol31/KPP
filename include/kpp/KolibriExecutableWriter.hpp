#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kpp {

class KolibriExecutableWriter
{
public:

    bool write(
        const std::string& filename,
        const std::vector<std::uint8_t>& code);

private:

    static constexpr std::uint32_t HeaderSize = 36;
    static constexpr std::uint32_t StackSize  = 4096;

    void write32(
        std::vector<std::uint8_t>& data,
        std::uint32_t value);
};

}
