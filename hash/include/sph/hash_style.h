#pragma once
#include <cstdint>

namespace sph
{
    enum class hash_style : uint8_t
    {
        append,
        append_padded,
        separate,
        separate_padded,
    };
}
