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
        template <typename T>
        concept is_one_byte = requires {
            sizeof(T) == 1;
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
            using iterator = detail::hash_iterator<R, T, A, S, detail::iterate_style::no_appended_hash>;
            using sentinel = detail::hash_sentinel<R, T, A, S, detail::iterate_style::no_appended_hash>;

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

        template<std::ranges::viewable_range R, typename T = uint8_t, sph::hash_algorithm A = sph::hash_algorithm::blake2b, sph::hash_format F, sph::hash_site S = sph::hash_site::separate>
        hash_view(R&&) -> hash_view<R, T, A, F, S>;

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
    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the requested hash function.
     *
     * @tparam A The hash algorithm to use. Either hash_algorithm::sha256,
     *      hash_algorithm::sha512, or hash_algorithm::blake2b.
     * @tparam T The output type. Must be std::is_standard_layout_v<T>.
     * @tparam F The hash format to use. Either raw or padded.
     * @tparam S The hash site to use. Either hash_site::append or
     *      hash_site::separate. Defaults to separate. With append, the input
     *      range gets passed to the output view with the hash immediately
     *      following.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that range.
     */
    template<sph::hash_algorithm A, typename T, sph::hash_format F, sph::hash_site S = sph::hash_site::separate>
        requires (std::is_standard_layout_v<T>)
    auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<T, A, F, S>
    {
        return sph::ranges::views::detail::hash_fn<T, A, F, S>{target_hash_size};
    }

    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the blake2b hash function.
     *
     * @tparam T The output type. Must be std::is_standard_layout_v<T>.
     * @tparam F The hash format to use. Either raw or padded.
     * @tparam S The hash site to use. Either hash_site::append or
     *      hash_site::separate. Defaults to separate. With append, the input
     *      range gets passed to the output view with the hash immediately
     *      following.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that range.
     */
    template<typename T, sph::hash_format F, sph::hash_site S = sph::hash_site::separate>
        requires (std::is_standard_layout_v<T>)
    auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<T, sph::hash_algorithm::blake2b, F, S>
    {
        return sph::ranges::views::detail::hash_fn<T, sph::hash_algorithm::blake2b, F, S>{target_hash_size};
    }

    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the requested hash function.
     *
     * The hash will be unpadded.
     *
     * @tparam A The hash algorithm to use. Either hash_algorithm::sha256,
     *      hash_algorithm::sha512, or hash_algorithm::blake2b.
     * @tparam T The one-byte output type.
     * @tparam S The hash site to use. Either hash_site::append or
     *      hash_site::separate. Defaults to separate. With append, the input
     *      range gets passed to the output view with the hash immediately
     *      following.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that range.
     */
    template<sph::hash_algorithm A, typename T, sph::hash_site S = sph::hash_site::separate>
        requires sph::ranges::views::detail::is_one_byte<T>
    auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<T, A, sph::hash_format::raw, S>
    {
        return sph::ranges::views::detail::hash_fn<T, A, sph::hash_format::raw, S>{target_hash_size};
    }

    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the blake2b hash function.
     *
     * The hash will be unpadded.
     *
     * @tparam T The one-byte output type.
     * @tparam S The hash site to use. Either hash_site::append or
     *      hash_site::separate. Defaults to separate. With append, the input
     *      range gets passed to the output view with the hash immediately
     *      following.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that range.
     */
    template<typename T, sph::hash_site S = sph::hash_site::separate>
        requires sph::ranges::views::detail::is_one_byte<T>
    auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<T, sph::hash_algorithm::blake2b, sph::hash_format::raw, S>
    {
        return sph::ranges::views::detail::hash_fn<T, sph::hash_algorithm::blake2b, sph::hash_format::raw, S>{target_hash_size};
    }

    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the requested hash function.
     *
     * The hash will be padded to fill the output type.
     *
     * @tparam A The hash algorithm to use. Either
     *      hash_algorithm::sha256, hash_algorithm::sha512, or
     *      hash_algorithm::blake2b.
     * @tparam T The multi-byte output type. Must be std::is_standard_layout_v<T>.
     * @tparam S The hash site to use. Either hash_site::append or
     *      hash_site::separate. Defaults to separate. With append, the input
     *      range gets passed to the output view with the hash immediately
     *      following.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that range.
     */
    template<sph::hash_algorithm A, typename T, sph::hash_site S = sph::hash_site::separate>
        requires (!sph::ranges::views::detail::is_one_byte<T> && std::is_standard_layout_v<T>)
    auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<T, A, sph::hash_format::padded, S>
    {
        return sph::ranges::views::detail::hash_fn<T, A, sph::hash_format::padded, S>{target_hash_size};
    }

    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the blake2b hash function.
     *
     * The hash will be padded to fill the output type.
     *
     * @tparam T The multi-byte output type. Must be std::is_standard_layout_v<T>.
     * @tparam S The hash site to use. Either hash_site::append or
     *      hash_site::separate. Defaults to separate. With append, the input
     *      range gets passed to the output view with the hash immediately
     *      following.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that range.
     */
    template<typename T, sph::hash_site S = sph::hash_site::separate>
        requires (!sph::ranges::views::detail::is_one_byte<T>&& std::is_standard_layout_v<T>)
    auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<T, sph::hash_algorithm::blake2b, sph::hash_format::padded, S>
    {
        return sph::ranges::views::detail::hash_fn<T, sph::hash_algorithm::blake2b, sph::hash_format::padded, S>{target_hash_size};
    }

    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the requested hash function.
     *
     * Outputs a range of uint8_t, the hash is raw (unpadded).
     *
     * @tparam A The hash algorithm to use. Either
     *      hash_algorithm::sha256, hash_algorithm::sha512, or
     *      hash_algorithm::blake2b.
     * @tparam S The hash site to use. Either hash_size::separate or
     *      hash_style::append. With append style, the input range gets passed
     *      to the output view with the hash immediately following.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that range.
     */
    template<sph::hash_algorithm A, sph::hash_site S>
    auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<uint8_t, A, sph::hash_format::raw, S>
    {
        return sph::ranges::views::detail::hash_fn<uint8_t, A, sph::hash_format::raw, S>{target_hash_size};
    }

    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the blake2b hash function.
     *
     * Outputs a range of uint8_t, the hash is raw (unpadded).
     *
     * @tparam S The hash site to use. Either hash_size::separate or
     *      hash_style::append. With append style, the input range gets passed
     *      to the output view with the hash immediately following.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that range.
     */
    template<sph::hash_site S>
    auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<uint8_t, sph::hash_algorithm::blake2b, sph::hash_format::raw, S>
    {
        return sph::ranges::views::detail::hash_fn<uint8_t, sph::hash_algorithm::blake2b, sph::hash_format::raw, S>{target_hash_size};
    }

    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the requested hash function.
     *
     * Outputs the hash as a range of uint8_t, the hash is raw (unpadded).
     *
     * @tparam A The hash algorithm to use. Either hash_algorithm::sha256,
     *      hash_algorithm::sha512, or hash_algorithm::blake2b. Defaults to
     *      blake2b.
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that
     *      range.
     */
    template<sph::hash_algorithm A = sph::hash_algorithm::blake2b>
    auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<uint8_t, A, sph::hash_format::raw, sph::hash_site::separate>
    {
        return sph::ranges::views::detail::hash_fn<uint8_t, A, sph::hash_format::raw, sph::hash_site::separate>{target_hash_size};
    }

    // ReSharper disable once CppDoxygenSyntaxError
    /**
     * A range adaptor that represents view of an underlying sequence after
     * applying the blake2b hash function.
     *
     * Outputs the hash as a range of uint8_t, the hash is raw (unpadded).
     *
     * @param target_hash_size The size in bytes of the hash to create.
     *      <code>0 <= target_hash_size <= sph::hash_param<A>::hash_byte_count()</code>
     *      or <code>std::invalid_argument</code>; if 0,
     *      <code>...hash_byte_count()</code> is used.
     * @return a functor that takes a range and returns a hashed view of that
     *      range.
     */
    inline auto hash(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_fn<uint8_t, sph::hash_algorithm::blake2b, sph::hash_format::raw, sph::hash_site::separate>
    {
        return sph::ranges::views::detail::hash_fn<uint8_t, sph::hash_algorithm::blake2b, sph::hash_format::raw, sph::hash_site::separate>{target_hash_size};
    }
}
