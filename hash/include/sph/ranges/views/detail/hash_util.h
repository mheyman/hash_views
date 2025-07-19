#pragma once
#include <tuple>
#include <type_traits>

namespace sph::ranges::views::detail
{
    template <typename T>
    concept hashable_type =
        std::is_standard_layout_v<T>
        && std::is_trivially_copyable_v<T>
        && !std::is_pointer_v<T>
        && !std::is_reference_v<T>;

    template <typename R>
    concept hash_range = std::ranges::viewable_range<std::remove_cvref_t<R>>
        && sph::ranges::views::detail::hashable_type<std::remove_cvref_t<std::ranges::range_value_t<R>>>;
}