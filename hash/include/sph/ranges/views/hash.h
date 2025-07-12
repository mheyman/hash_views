#pragma once
#include <ranges>
#include <sph/hash_algorithm.h>
#include <sph/hash_format.h>
#include <sph/hash_site.h>
#include <sph/ranges/views/detail/hash_iterator.h>
#include <sph/ranges/views/detail/get_hash_size.h>

namespace sph::ranges::views
{
    namespace detail
    {

        template<typename T>
        [[nodiscard]] constexpr bool is_one_byte_v = sizeof(T) == 1;

        template<typename T>
        [[nodiscard]] constexpr bool is_hash_enum_v =
            std::is_same_v<T, sph::hash_algorithm> ||
            std::is_same_v<T, sph::hash_format> ||
            std::is_same_v<T, sph::hash_site>;
        static_assert(is_hash_enum_v<sph::hash_algorithm>);
        static_assert(is_hash_enum_v<sph::hash_format>);
        static_assert(is_hash_enum_v<sph::hash_site>);

        template<typename T>
        constexpr bool is_output_type_v =
            std::is_standard_layout_v<T> &&
            std::is_trivially_copyable_v<T> &&
            !std::is_pointer_v<T> &&
            !std::is_reference_v<T> &&
            !is_hash_enum_v<T>;

        template<typename T>
        using default_hash_format = std::conditional_t<
            is_one_byte_v<T>,
            std::integral_constant<sph::hash_format, sph::hash_format::raw>,
            std::integral_constant<sph::hash_format, sph::hash_format::padded>
        >;

        // Default output type if walking the types doesn't find an output type.
        template<typename... Args>
        struct hash_pick_output_type {
            using type = uint8_t; // default
        };

        // Walk the types and pick the first one that is an output type.
        template<typename First, typename... Rest>
        struct hash_pick_output_type<First, Rest...> {
            using type = std::conditional_t<
                is_output_type_v<First>,
                First,
                typename hash_pick_output_type<Rest...>::type
            >;
        };


        // unset enum value.
        struct hash_enum_type_not_specified {};

        // Default hash enum type if walking the types doesn't find a desired enum.
        template<typename Want, typename... Args>
        struct hash_pick_enum_type { using type = hash_enum_type_not_specified; };

        // Walk the types and pick the first enum that is the type wanted.
        template<typename Want, typename First, typename... Rest>
        struct hash_pick_enum_type<Want, First, Rest...> {
            using type = std::conditional_t<
                std::is_same_v<Want, First>, First, typename hash_pick_enum_type<Want, Rest...>::type
            >;
        };

        // Deduce the types needed for the hash_view from the hash parameters.
        template<typename... Args>
        struct hash_view_deduced_param {
            using output_type = typename hash_pick_output_type<Args...>::type;
            using algorithm = std::conditional_t<
                std::is_same_v<typename hash_pick_enum_type<sph::hash_algorithm, Args...>::type, hash_enum_type_not_specified>,
                std::integral_constant<sph::hash_algorithm, sph::hash_algorithm::blake2b>,
                typename hash_pick_enum_type<sph::hash_algorithm, Args...>::type
            >;
            using site = 
                std::conditional_t<
                std::is_same_v<typename hash_pick_enum_type<sph::hash_site, Args...>::type, hash_enum_type_not_specified>,
                std::integral_constant<sph::hash_site, sph::hash_site::separate>,
                typename hash_pick_enum_type<sph::hash_site, Args...>::type
                >;
            using format = std::conditional_t<
                std::is_same_v<typename hash_pick_enum_type<sph::hash_format, Args...>::type, hash_enum_type_not_specified>,
                default_hash_format<typename hash_pick_output_type<Args...>::type>,
                typename hash_pick_enum_type<sph::hash_format, Args...>::type
            >;
        };

        /**
         * @brief A view that encodes binary data into hashed data.
         * @tparam R The type of the range that holds a hashed stream.
         * @tparam T The output type.
         * @tparam A The hash algorithm to use.
         * @tparam S The hash style to use (append to hashed data or separate from hashed data).
         */
        template<std::ranges::viewable_range R, typename T, sph::hash_algorithm A, sph::hash_format F, sph::hash_site S>
            requires std::ranges::input_range<R> && std::is_standard_layout_v<T> && std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
        class hash_view : public std::ranges::view_interface<hash_view<R, T, A, F, S>> {
            R input_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
            size_t target_hash_size_;
        public:
            using iterator = hash_iterator<R, T, A, F, S, end_of_input::no_appended_hash>;
            using sentinel = hash_sentinel<R, T, A, F, S, end_of_input::no_appended_hash>;

            /**
             * Initialize a new instance of the hash_view class.
             *
             * Provides a begin() iterator and end() sentinel over the
             * hashed view of the given input range.
             *
             * @param target_hash_size Hash will be this size for single byte
             * output; it may extend to fill multibyte output.
             * @param input the range to hash.
             */
            explicit hash_view(size_t target_hash_size, R&& input) noexcept
                : input_(std::forward<R>(input)), target_hash_size_{ detail::get_hash_size<A>(target_hash_size) } {}

            hash_view(hash_view const&) = default;
            hash_view(hash_view&&) noexcept = default;
            ~hash_view() noexcept = default;
            auto operator=(hash_view const&) -> hash_view& = default;
            auto operator=(hash_view&&) noexcept -> hash_view& = default;

            auto begin() const -> iterator { return iterator(std::ranges::begin(input_), std::ranges::end(input_), target_hash_size_); }
            // ReSharper disable once CppMemberFunctionMayBeStatic
            auto end() const -> sentinel { return sentinel{}; }
        };

        /**
         * Functor that, given a range, provides a hashed view of that range.
         * @tparam T The type to hash into.
         */
        template <typename T, sph::hash_algorithm A, sph::hash_format F, sph::hash_site S>
        class hash_fn : public std::ranges::range_adaptor_closure<hash_fn<T, A, F, S>>
        {
            size_t target_hash_size_;
        public:
            explicit hash_fn(size_t target_hash_size) noexcept : target_hash_size_{ target_hash_size } {}
            template <std::ranges::viewable_range R>
            [[nodiscard]] constexpr auto operator()(R&& range) const -> hash_view<std::views::all_t<R>, T, A, F, S>
            {
                return hash_view<std::views::all_t<R>, T, A, F, S>(target_hash_size_, std::views::all(std::forward<R>(range)));
            }
        };
    }
}

namespace sph::views
{
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the requested hash function.
     *
     * @tparam Args The output type, hash algorithm, hash format, and hash
     *      site. Any order works and there are defaults. The output type must
     *      be standard layout and defaults to uint8_t. The hash algorithm is
     *      one of hash_algorithm::sha256, hash_algorithm::sha512, or
     *      hash_algorithm::blake2b and defaults to blake2b. The hash format is
     *      either raw or padded and defaults to raw for 1-byte output types
     *      and padded for multi-byte output types. The hash site is either
     *      hash_site::append or hash_site::separate and defaults to separate
     *      where only the hash bytes are output. With append, the input range
     *      gets passed to the output view with the hash immediately following.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that
     *      range. Intended for use in a pipe expression.
     */
    template<typename... Args>
    auto hash(size_t target_hash_size = 0)
    {
        using param = sph::ranges::views::detail::hash_view_deduced_param<Args...>;
        return sph::ranges::views::detail::hash_fn<
            typename param::output_type,
            typename param::algorithm,
            typename param::format,
            typename param::site
        >{target_hash_size};
    }
}
