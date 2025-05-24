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

    template<typename T>
    concept hash_size_members = requires(T instance) {
        { T::hash_size } -> std::same_as<size_t const&>;
        { T::chunk_size } -> std::same_as<size_t const&>;
    };

    template<typename T>
    concept basic_hash = hash_size_members<T> && requires(T hash, std::span<uint8_t const, T::chunk_size> const chunk, std::span<uint8_t const> const final_data)
    {
        { T{ std::declval<size_t>() } };
        { hash.target_hash_size() } -> std::same_as<size_t>;
        { hash.hash() } -> std::same_as<std::span<const uint8_t, T::hash_size>>;
        { hash.update(chunk) } -> std::same_as<void>;
        { hash.final(final_data) } -> std::same_as<void>;
    };
}