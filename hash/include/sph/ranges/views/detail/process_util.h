#pragma once
#include <tuple>
#include <type_traits>

namespace sph::ranges::views::detail
{
    template <typename T>
    concept next_byte_function = requires(T t)
    {
        { t() } -> std::same_as<std::tuple<bool, uint8_t>>;
    };
}