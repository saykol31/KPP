#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kpp {

class BinaryWriter
{
public:

    bool write(
        const std::string& filename,
        const std::vector<std::uint8_t>& data);

};

}
