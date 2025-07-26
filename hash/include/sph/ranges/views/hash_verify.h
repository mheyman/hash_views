#pragma once
#include <algorithm>
#include <ranges>
#include <sph/hash_algorithm.h>
#include <sph/hash_site.h>
#include <sph/hash_format.h>
#include <sph/ranges/views/detail/hash_util.h>
#include <sph/ranges/views/detail/single_bool_iterator.h>

namespace sph::ranges::views::detail
{
    struct hash_verify_empty {};

    template <typename R>
    concept hash_verify_single_byte_range = hash_range<R> &&
        (sizeof(std::remove_cvref_t<std::ranges::range_value_t<R>>) == 1);

    template <typename R>
    concept hash_verify_multi_byte_range = hash_range<R> &&
        (sizeof(std::remove_cvref_t<std::ranges::range_value_t<R>>) > 1);

    /**
     * Concept for a range or std::nullopt_t.
     * @tparam H The type of the range.
     */
    template <typename H>
    concept hash_verify_fn_hash_range = hash_range<H> || std::is_same_v<H, std::nullopt_t>;

    /**
     * Type declaring a padded hash. Used instead of enum to allow for default behavior if unset.
     */
    using hash_verify_format_padded = std::integral_constant<sph::hash_format, sph::hash_format::padded>;

    /**
     * Type declaring a raw hash. Used instead of enum to allow for default behavior if unset.
     */
    using hash_verify_format_raw = std::integral_constant<sph::hash_format, sph::hash_format::raw>;

    /**
     * Convert a hash format enum to the associated type.
     * @tparam F The hash format enum value.
     */
    template <sph::hash_format F>
    using hash_verify_format_t = 
    std::conditional_t<F == sph::hash_format::padded, sph::ranges::views::detail::hash_verify_format_padded, sph::ranges::views::detail::hash_verify_format_raw>;


    /**
     * A hash_verify_format_padded, hash_verify_format_raw or std::nullopt_t.
     * @tparam F the type of the hash format.
     */
    template <typename F>
    concept hash_verify_fn_hash_format = std::is_same_v<F, sph::ranges::views::detail::hash_verify_format_padded> || std::is_same_v<F, sph::ranges::views::detail::hash_verify_format_raw> || std::is_same_v<F, std::nullopt_t>;

    /**
     * Get the hash_format value from the hash_verify_fn_hash_format type.
     * @tparam F The type of the hash format.
     * @tparam R The range holding a hash.
     */
    template<hash_verify_fn_hash_format F, typename R>
    struct hash_verify_format
    {
        static constexpr sph::hash_format value = std::conditional_t<
            std::is_same_v<F, std::nullopt_t>,
                std::conditional_t<
                    hash_verify_single_byte_range<R>,
                        hash_verify_format_raw,
                        hash_verify_format_padded>,
                F>();
    };


    /**
     * Helper to get the hash output type from the input range and the hash range.
     *
     * Undefined for the primary case.
     *
     * @tparam R The range holding the data to hash.
     * @tparam H The range holding the hash or hash_verify_empty if the hash is appended to the input range.
     */
    template <hash_range R, typename H, typename = void>
    struct hash_verify_output_helper;

    /**
     * Helper to get the hash output type from the input range and the hash range.
     *
     * Specialization for H is hash_verify_empty.
     *
     * @tparam R The range holding the data to hash.
     */
    template <hash_range R>
    struct hash_verify_output_helper<R, hash_verify_empty> {
        using type = std::ranges::range_value_t<R>;
    };

    /**
     * Get the hash output type from the input range and the hash range.
     *
     * Specialization for H is not hash_verify_empty.
     *
     * @tparam R The range holding the data to hash.
     * @tparam H The range holding the hash or hash_verify_empty if the hash is appended to the input range.
     */
    template <hash_range R, hash_range H>
    struct hash_verify_output_helper<R, H, std::void_t<std::ranges::range_value_t<H>>> {
        using type = std::ranges::range_value_t<H>;
    };

    /**
     * Get the hash output type from the input range and the hash range.
     *
     * @tparam R The range holding the data to hash.
     * @tparam H The range holding the hash or hash_verify_empty if the hash is appended to the input range.
     */
    template <hash_range R, typename H>
    using hash_verify_output = typename hash_verify_output_helper<R, H>::type;
        
    /**
         * Provides view of hash of the input range.
         * @tparam R The type of the range that holds a hashed stream.
         * @tparam T The type of the hash output range elements.
         * @tparam A The type of the hash to do.
         * @tparam F The format of the hash (padded or raw).
         */
    template<hash_range R, hashable_type T, sph::hash_algorithm A, sph::hash_format F>
    class hash_verify_view : public std::ranges::view_interface<hash_verify_view<R, T, A, F>>
    {
        bool verify_ok_{ false };

        using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
        using input_append_iterator = detail::hash_iterator<R, input_type, A, F, sph::hash_site::append, end_of_input::skip_appended_hash>;
        using input_append_sentinel = detail::hash_sentinel<R, input_type, A, F, sph::hash_site::append, end_of_input::skip_appended_hash>;
        using input_separate_iterator = detail::hash_iterator<R, T, A, F, sph::hash_site::separate, end_of_input::no_appended_hash>;
        using input_separate_sentinel = detail::hash_sentinel<R, T, A, F, sph::hash_site::separate, end_of_input::no_appended_hash>;
    public:
        using iterator = single_bool_iterator;
        using sentinel = single_bool_sentinel;

        /**
         * Initialize a new instance of the hash_verify_view class.
         *
         * Provides a begin() iterator and end() sentinel over the
         * verified view of the given input range.
         *
         * @tparam H The type of the range that holds the hash to compare against.
         * @param input the range to verify.
         * @param hash the hash to compare against.
         */
        template<hash_range H>
        hash_verify_view(R&& input, H&& hash)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
            : verify_ok_{verify(std::forward<R>(input), std::forward<H>(hash))}
        {}

        /**
         * Initialize a new instance of the hash_verify_view class.
         *
         * Provides a begin() iterator and end() sentinel over the
         * verified view of the given input range.
         *
         * The hash is appended to the input range.
         *
         * @param target_hash_size the size (in bytes) of the hash.
         * @param input the range to verify.
         */
        hash_verify_view(size_t target_hash_size, R&& input)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
            : verify_ok_ { verify(target_hash_size, std::forward<R>(input)) }
        {}

        hash_verify_view(hash_verify_view const&) = default;
        hash_verify_view(hash_verify_view&&) = default;
        hash_verify_view() = default;
        ~hash_verify_view() noexcept = default;
        auto operator=(hash_verify_view const&) -> hash_verify_view& = default;
        auto operator=(hash_verify_view&&o) noexcept -> hash_verify_view& = default;


        [[nodiscard]] auto begin() -> iterator { return iterator(verify_ok_); }
        [[nodiscard]] auto begin() const -> iterator { return iterator(verify_ok_); }

        [[nodiscard]] auto end() -> sentinel { return sentinel{}; }
        [[nodiscard]] auto end() const -> sentinel { return sentinel{}; }
    private:

        /**
         * Get the unpadded length of a padded hash or just the length from an unpadded hash.
         * @param maybe_padded_hash the hash to check.
         * @return the unpadded length of the hash.
         */
        static auto maybe_unpadded_length(std::vector<uint8_t> const& maybe_padded_hash) -> size_t
        {
            if constexpr (F == sph::hash_format::raw)
            {
                return maybe_padded_hash.size();
            }
            else
            {
                // requires that maybe_padded_hash is actually padded - that is, it contains a 0x80 byte followed by zero or more 0x00 bytes.
                return static_cast<size_t>(
                    std::ranges::distance(
                        &*maybe_padded_hash.begin(),
                        &*std::ranges::find(maybe_padded_hash | std::views::reverse, 0x80)));
            }
        }

        /**
         * Convert hash into a vector of uint8_t.
         */
        template<hash_verify_single_byte_range H>
        static constexpr auto hash_to_byte_vector(H&& hash) -> std::tuple<std::vector<uint8_t>, size_t>
        {
            using value_t = std::remove_cvref_t<std::ranges::range_value_t<H>>;
            auto ret {std::forward<H>(hash)
                | std::views::transform([](value_t v) -> uint8_t
                {
                    return static_cast<uint8_t>(v);
                })
                | std::ranges::to<std::vector>()};
                return { ret, maybe_unpadded_length(ret) };
        }

        template<hash_verify_multi_byte_range H>
        static constexpr auto hash_to_byte_vector(H&& hash) -> std::tuple<std::vector<uint8_t>, size_t>
        {
            using value_t = std::remove_cvref_t<std::ranges::range_value_t<H>>;
            auto ret {std::forward<H>(hash)
                | std::views::transform([](value_t v) -> std::array<uint8_t, sizeof(value_t)>
                {
                    std::array<uint8_t, sizeof(value_t)> tmp{};
                    std::copy_n(reinterpret_cast<uint8_t*>(&v), sizeof(value_t), tmp.data());
                    return tmp;
                })
                | std::views::join
                | std::ranges::to<std::vector>()};
            return { ret, maybe_unpadded_length(ret) };
        }

        template<hash_range H>
        static auto verify(R&& input, H&& hash) -> bool
        {
            auto [provided_hash, target_hash_size]{ hash_to_byte_vector(std::forward<H>(hash)) };
            R to_hash{ std::move(input) };
            auto [hash_result, result_hash_size]
            { 
                hash_to_byte_vector(
                    std::ranges::subrange(input_separate_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size), input_separate_sentinel{}))
            };
            if (provided_hash.size() != hash_result.size())
            {
                fmt::print("H size mismatch: expected {} bytes, calculated {} bytes.\n  expected {},\n       got {}\n",
                    provided_hash.size(),
                    hash_result.size(),
                    fmt::join(provided_hash | std::views::transform([](auto x) -> std::string { return fmt::format("{:02X}", static_cast<uint8_t>(x)); }), " "),
                    fmt::join(hash_result | std::views::transform([](auto x) -> std::string { return fmt::format("{:02X}", static_cast<uint8_t>(x)); }), " "));
                hash_to_byte_vector(
                    std::ranges::subrange(input_separate_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size), input_separate_sentinel{}));
                    return false;
            }

            if (target_hash_size != result_hash_size)
            {
                fmt::print("H target size mismatch: expected {} bytes, calculated {} bytes.\n  expected {},\n       got {}\n",
                    target_hash_size,
                    result_hash_size,
                    fmt::join(provided_hash | std::views::transform([](auto x) -> std::string { return fmt::format("{:02X}", static_cast<uint8_t>(x)); }), " "),
                    fmt::join(hash_result | std::views::transform([](auto x) -> std::string { return fmt::format("{:02X}", static_cast<uint8_t>(x)); }), " "));
                return false;
            }

            bool const ret{ std::ranges::all_of(std::views::zip(provided_hash | std::views::take(target_hash_size), hash_result | std::views::take(target_hash_size)), [](auto&& pair)
            {
                return std::get<0>(pair) == std::get<1>(pair);
            })};
            if constexpr (sizeof(std::remove_cvref_t<std::ranges::range_value_t<H>>) == 1)
            {
                if (!ret)
                {
                    fmt::print("H mismatch: provided hash does not match computed hash: provided: {}, calculated: {}.\n",
                               fmt::join(provided_hash | std::views::transform([](auto x) -> std::string { return fmt::format("{:02X}", static_cast<uint8_t>(x)); }), " "),
                               fmt::join(hash_result | std::views::transform([](auto x) -> std::string { return fmt::format("{:02X}", static_cast<uint8_t>(x)); }), " ")
                    );
                }
            }

            return ret;
        }

        static auto verify(size_t target_hash_size, R&& input) -> bool
        {
            R to_hash{ std::move(input) };
            auto hasher { input_append_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size) };
            auto  [hash_result, hash_result_size]{ hash_to_byte_vector(std::ranges::subrange(hasher, input_append_sentinel{})) };
            auto [appended_hash, appended_hash_size] { hash_to_byte_vector(hasher.hash()) }; // available after iteration complete
            if (appended_hash.size() != hash_result.size())
            {
                return false;
            }

            return std::ranges::all_of(std::views::zip(appended_hash, hash_result), [](auto&& pair)
            {
                return std::get<0>(pair) == std::get<1>(pair);
            });
        }
    };

    template <typename>
    struct hash_verify_always_false : std::false_type {};

    /**
     * Functor that, given a hashed+hash range or hashed range and a 
     * separate hash range, provides a range of a single boolean indicating
     * if the hash verified correctly or not.
     * @tparam A The hash algorithm.
     * @tparam H The hash data range type to verify against.
     */
    template <sph::hash_algorithm A, hash_verify_fn_hash_format F, hash_verify_fn_hash_range H>
    class hash_verify_fn : public std::ranges::range_adaptor_closure<hash_verify_fn<A, F, H>>
    {
        static constexpr bool appended_hash{ std::is_same_v<H, std::nullopt_t> };
        using hash_t = std::conditional_t<appended_hash, hash_verify_empty, H>;
        using target_hash_size_t = std::conditional_t<appended_hash, size_t, hash_verify_empty>;
        hash_t hash_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        target_hash_size_t  target_hash_size_;

    public:
        explicit hash_verify_fn(size_t target_hash_size = 0) : target_hash_size_{target_hash_size} {}

        explicit hash_verify_fn(H&& hash)
            : hash_{std::forward<H>(hash)} {}

        template <hash_range R>
        [[nodiscard]] constexpr auto operator()(R&& range) const
            -> hash_verify_view< std::views::all_t<R>, hash_verify_output<R, hash_t>, A, hash_verify_format<F, R>::value>
            requires (appended_hash && sph::ranges::views::detail::copyable_or_borrowed<R>)
        {
            return hash_verify_view<std::views::all_t<R>, hash_verify_output<R, hash_t>, A, hash_verify_format<F, R>::value>(
                target_hash_size_, std::views::all(std::forward<R>(range)));
        }

        template <hash_range R>
        [[nodiscard]] constexpr auto operator()(R&& range) const
            -> hash_verify_view<std::views::all_t<R>, hash_verify_output<R, hash_t>, A, hash_verify_format<F, hash_t>::value>
            requires (!appended_hash && sph::ranges::views::detail::copyable_or_borrowed<hash_t>)
        {
            return hash_verify_view<std::views::all_t<R>, hash_verify_output<R, hash_t>, A, hash_verify_format<F, hash_t>::value>(
                std::views::all(std::forward<R>(range)), std::views::all(hash_));
        }

        template <hash_range R>
        [[nodiscard]] constexpr auto operator()(R&& range) const-> std::array<bool, 0>
            requires (appended_hash && !sph::ranges::views::detail::copyable_or_borrowed<R>)
        {
            // Here, a copy could be made and, maybe, if a low-cost method
            // exists, that should get done here.
            static_assert(hash_verify_always_false<R>::value,
                "The range with appended hash supplied to hash_verify must be "
                "copyable or a borrowed range. If you have a move-only range "
                "(like owning_view), make a copy (like:  "
                "`range_with_appended_hash | std::ranges::to<std::vector>() | "
                "hash_verify()`).");
            return {};
        }
    };

}

/**
 * hash_verify_view's return single_bool_iterator which doesn't have a
 * reference to the hash_verify_view so it is safe to use as a borrowed range.
 * @tparam R The type of the range that holds a hashed stream.
 * @tparam T The type of the output range that holds a hashed stream.
 * @tparam A The type of the hash to do.
 * @tparam F The format of the hash (padded or raw).
 */
template <sph::ranges::views::detail::hash_range R, sph::ranges::views::detail::hashable_type T, sph::hash_algorithm A, sph::hash_format F>
inline constexpr bool std::ranges::enable_borrowed_range<sph::ranges::views::detail::hash_verify_view<R, T, A, F>> = true;

namespace sph::views
{
	/**
     * A range adaptor that represents view of an underlying single-element sequence of the hash verification.
     *
     * @tparam A The hash algorithm
     * @param target_hash_size the minimum size in bytes of the hash created.
     * @return A functor that takes a hashed+hash range or separate hashed and hash ranges and returns a view of the verification status.
	 */
	template<sph::hash_algorithm A, sph::hash_format F>
    auto hash_verify(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, std::nullopt_t>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, std::nullopt_t>{target_hash_size};
    }

    template<sph::hash_format F, sph::hash_algorithm A>
    auto hash_verify(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, std::nullopt_t>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, std::nullopt_t>{target_hash_size};
    }

    template<sph::hash_algorithm A>
    auto hash_verify(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<A, std::nullopt_t, std::nullopt_t>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, std::nullopt_t, std::nullopt_t>{target_hash_size};
    }

    template<sph::hash_format F>
    auto hash_verify(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, ranges::views::detail::hash_verify_format_t<F>, std::nullopt_t>
    {
        return sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, ranges::views::detail::hash_verify_format_t<F>, std::nullopt_t>{target_hash_size};
    }

    inline auto hash_verify(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, std::nullopt_t, std::nullopt_t>
    {
        return sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, std::nullopt_t, std::nullopt_t>{target_hash_size};
    }


    /**
     * A range adaptor that represents view of an underlying single-element sequence of the hash verification.
     *
     * @tparam A The hash algorithm
     * @param hash the range holding the hash to compare against.
     * @return A functor that takes a hashed+hash range or separate hashed and hash ranges and returns a view of the verification status.
     */
    template<sph::hash_algorithm A, sph::hash_format F, sph::ranges::views::detail::hash_range H>
    auto hash_verify(H&& hash) -> sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, H>{std::forward<H>(hash)};
    }

    template<sph::hash_format F, sph::hash_algorithm A, sph::ranges::views::detail::hash_range H>
    auto hash_verify(H&& hash) -> sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, H>{std::forward<H>(hash)};
    }

    template<sph::hash_algorithm A, sph::ranges::views::detail::hash_range H>
    auto hash_verify(H&& hash) -> sph::ranges::views::detail::hash_verify_fn<A, std::nullopt_t, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, std::nullopt_t, H>{std::forward<H>(hash)};
    }

    template<sph::hash_format F, sph::ranges::views::detail::hash_range H>
    auto hash_verify(H&& hash) -> sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, ranges::views::detail::hash_verify_format_t<F>, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, ranges::views::detail::hash_verify_format_t<F>, H>{std::forward<H>(hash)};
    }

    template<sph::ranges::views::detail::hash_range H>
    auto hash_verify(H&& hash) -> sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, std::nullopt_t, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, std::nullopt_t, H>{std::forward<H>(hash)};
    }

}

