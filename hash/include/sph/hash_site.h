#pragma once
#include <cstdint>

namespace sph
{
    // Note: because of the way MSVC mangles value types in templated methods,
    // the enums must be defined so the hash_algorithm, hash_format, and
    // hash_site are all unique.

    /**
     * @brief Enum representing the site of the hash.
     */
    enum class hash_site : uint8_t
    {
        append = 21,
        separate = 22,
    };
}
