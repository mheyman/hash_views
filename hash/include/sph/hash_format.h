#pragma once
#include <cstdint>

namespace sph
{
    // Note: because of the way MSVC mangles value types in templated methods,
    // the enums must be defined so the hash_algorithm, hash_format, and
    // hash_site are all unique.
    /**
     * @brief Enum representing the format of the hash.
     */
    enum class hash_format : uint8_t
    {
        raw = 10,
        padded = 11,
    };
}
