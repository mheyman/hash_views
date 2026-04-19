#pragma once
#include <cstdint>
#include <span>
#include <type_traits>
#include <sph/hash_algorithm.h>

namespace sph
{
    struct blake2b_parameters
    {
        std::span<uint8_t const> key{};
        std::span<uint8_t const> salt{};
        std::span<uint8_t const> personal{};
    };
}

namespace sph::ranges::views::detail
{
    struct no_algorithm_parameters {};

    template <sph::hash_algorithm A>
    using algorithm_parameters_t = std::conditional_t<A == sph::hash_algorithm::blake2b, sph::blake2b_parameters, no_algorithm_parameters>;
}
