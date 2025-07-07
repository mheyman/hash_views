#pragma once
#include <cstdint>

namespace sph
{
    enum class hash_algorithm : uint8_t
    {
        sha256,
        sha512,
        blake2b,
    };
}