#pragma once
#include <cstdint>

namespace sph
{
    // Note: because of the way MSVC mangles value types in templated methods,
    // the enums must be defined so the hash_algorithm, hash_format, and
    // hash_site are all unique.
    /**
     * @brief Enum representing the hash algorithm.
     */
    enum class hash_algorithm : uint8_t
    {
        sha256 = 0,
        sha512 = 1,
        blake2b = 2,
    };
}