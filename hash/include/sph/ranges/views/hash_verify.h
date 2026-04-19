#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <sph/blake2b_parameters.h>
#include <sph/hash_algorithm.h>
#include <sph/hash_site.h>
#include <sph/hash_format.h>
#include <sph/ranges/views/detail/hash_util.h>
#include <sph/ranges/views/detail/single_bool_iterator.h>
#include <sph/ranges/views/hash.h>

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

    // Minimal operator| overloads for hash_view. Use if constexpr to
    // prefer calling the adaptor with the hash_view directly when
    // possible, otherwise forward a view produced by std::views::all.
    template <typename R, typename T, sph::hash_algorithm A, sph::hash_format F, sph::hash_site S, typename Fn>
    [[nodiscard]] inline decltype(auto) operator|(hash_view<R, T, A, F, S> && lhs, Fn && fn)
    {
        if constexpr (std::invocable<Fn, hash_view<R, T, A, F, S>&&>)
        {
            return std::forward<Fn>(fn)(std::move(lhs));
        }
        else
        {
            return std::forward<Fn>(fn)(std::views::all(std::move(lhs)));
        }
    }

    template <typename R, typename T, sph::hash_algorithm A, sph::hash_format F, sph::hash_site S, typename Fn>
    [[nodiscard]] inline decltype(auto) operator|(hash_view<R, T, A, F, S> const & lhs, Fn && fn)
    {
        if constexpr (std::invocable<Fn, hash_view<R, T, A, F, S> const &>)
        {
            return std::forward<Fn>(fn)(lhs);
        }
        else
        {
            return std::forward<Fn>(fn)(std::views::all(lhs));
        }
    }


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

        using algorithm_parameters_t = sph::ranges::views::detail::algorithm_parameters_t<A>;
        using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
        using input_append_iterator = detail::hash_iterator<R, input_type, A, F, sph::hash_site::append, end_of_input::skip_appended_hash>;
        using input_append_sentinel = detail::hash_sentinel<R, input_type, A, F, sph::hash_site::append, end_of_input::skip_appended_hash>;
        using input_separate_iterator = detail::hash_iterator<R, T, A, F, sph::hash_site::separate, end_of_input::no_appended_hash>;
        using input_separate_sentinel = detail::hash_sentinel<R, T, A, F, sph::hash_site::separate, end_of_input::no_appended_hash>;
        struct hash_bytes
        {
            std::vector<uint8_t> bytes;
            size_t target_hash_size;
            bool valid_padding;
        };
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

        template<hash_range H>
        hash_verify_view(R&& input, H&& hash, algorithm_parameters_t algorithm_parameters)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
            requires (A == sph::hash_algorithm::blake2b)
            : verify_ok_{verify(std::forward<R>(input), std::forward<H>(hash), algorithm_parameters)}
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

        hash_verify_view(size_t target_hash_size, R&& input, algorithm_parameters_t algorithm_parameters)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
            requires (A == sph::hash_algorithm::blake2b)
            : verify_ok_ { verify(target_hash_size, std::forward<R>(input), algorithm_parameters) }
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
        static auto maybe_unpadded_length(std::vector<uint8_t> const& maybe_padded_hash) -> std::pair<size_t, bool>
        {
            if constexpr (F == sph::hash_format::raw)
            {
                return { maybe_padded_hash.size(), true };
            }
            else
            {
                size_t current{ maybe_padded_hash.size() };
                while (current > 0 && maybe_padded_hash[current - 1] == 0x00)
                {
                    --current;
                }

                if (current == 0 || maybe_padded_hash[current - 1] != 0x80)
                {
                    return { 0, false };
                }

                return { current - 1, true };
            }
        }

        /**
         * Convert hash into a vector of uint8_t.
         */
        template<hash_verify_single_byte_range H>
        static constexpr auto hash_to_byte_vector(H&& hash) -> hash_bytes
        {
            using value_t = std::remove_cvref_t<std::ranges::range_value_t<H>>;
            auto ret {std::forward<H>(hash)
                | std::views::transform([](value_t v) -> uint8_t
                {
                    return static_cast<uint8_t>(v);
                })
                | std::ranges::to<std::vector>()};
            auto [target_hash_size, valid_padding]{ maybe_unpadded_length(ret) };
            return { std::move(ret), target_hash_size, valid_padding };
        }

        template<hash_verify_multi_byte_range H>
        static constexpr auto hash_to_byte_vector(H&& hash) -> hash_bytes
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
            auto [target_hash_size, valid_padding]{ maybe_unpadded_length(ret) };
            return { std::move(ret), target_hash_size, valid_padding };
        }

        template<hash_range H>
        static auto verify(R&& input, H&& hash) -> bool
        {
            return verify(std::forward<R>(input), std::forward<H>(hash), algorithm_parameters_t{});
        }

        template<hash_range H>
        static auto verify(R&& input, H&& hash, algorithm_parameters_t algorithm_parameters) -> bool
        {
            auto provided_hash_data{ hash_to_byte_vector(std::forward<H>(hash)) };
            auto const& provided_hash{ provided_hash_data.bytes };
            auto const target_hash_size{ provided_hash_data.target_hash_size };
            if (!provided_hash_data.valid_padding)
            {
                return false;
            }

            R to_hash{ std::move(input) };
            auto hash_result_data{
                [&]() -> hash_bytes
                {
                    if constexpr (A == sph::hash_algorithm::blake2b)
                    {
                        return hash_to_byte_vector(
                            std::ranges::subrange(
                                input_separate_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size, algorithm_parameters),
                                input_separate_sentinel{}));
                    }
                    else
                    {
                        return hash_to_byte_vector(
                            std::ranges::subrange(
                                input_separate_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size),
                                input_separate_sentinel{}));
                    }
                }()
            };
            auto const& hash_result{ hash_result_data.bytes };
            auto const result_hash_size{ hash_result_data.target_hash_size };
            if (!hash_result_data.valid_padding)
            {
                return false;
            }

            if (provided_hash.size() != hash_result.size())
            {
                return false;
            }

            if (target_hash_size != result_hash_size)
            {
                return false;
            }

            return { std::ranges::all_of(std::views::zip(provided_hash | std::views::take(target_hash_size), hash_result | std::views::take(target_hash_size)), [](auto&& pair)
            {
                return std::get<0>(pair) == std::get<1>(pair);
            })};
        }

        static auto verify(size_t target_hash_size, R&& input) -> bool
        {
            return verify(target_hash_size, std::forward<R>(input), algorithm_parameters_t{});
        }

        static auto verify(size_t target_hash_size, R&& input, algorithm_parameters_t algorithm_parameters) -> bool
        {
            R to_hash{ std::move(input) };
            auto hasher {
                [&]() -> input_append_iterator
                {
                    if constexpr (A == sph::hash_algorithm::blake2b)
                    {
                        return input_append_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size, algorithm_parameters);
                    }
                    else
                    {
                        return input_append_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size);
                    }
                }()
            };
            std::vector<input_type> unhashed_input{};
            for (; hasher != input_append_sentinel{}; ++hasher)
            {
                unhashed_input.push_back(*hasher);
            }

            auto appended_hash_data{ hash_to_byte_vector(hasher.appended_hash()) };
            auto const& appended_hash{ appended_hash_data.bytes };
            auto const appended_hash_size{ appended_hash_data.target_hash_size };
            if (!appended_hash_data.valid_padding)
            {
                return false;
            }

            auto hash_result_data{
                [&]() -> hash_bytes
                {
                    if constexpr (A == sph::hash_algorithm::blake2b)
                    {
                        auto hash_fn{
                            sph::views::hash<A, uint8_t, F>(target_hash_size)
                            .with_blake2b_parameters(algorithm_parameters)
                        };
                        return hash_to_byte_vector(unhashed_input | hash_fn);
                    }
                    else
                    {
                        return hash_to_byte_vector(unhashed_input | sph::views::hash<A, uint8_t, F>(target_hash_size));
                    }
                }()
            };
            auto const& hash_result{ hash_result_data.bytes };
            auto const hash_result_size{ hash_result_data.target_hash_size };
            if (!hash_result_data.valid_padding)
            {
                return false;
            }

            if (appended_hash_size != hash_result_size)
            {
                return false;
            }

            bool const ret{
                std::ranges::all_of(
                    std::views::zip(
                        appended_hash | std::views::take(appended_hash_size),
                        hash_result | std::views::take(hash_result_size)),
                    [](auto&& pair)
            {
                return std::get<0>(pair) == std::get<1>(pair);
            }) };
            return ret;
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
        using algorithm_parameters_t = sph::ranges::views::detail::algorithm_parameters_t<A>;
        static constexpr hash_format hf{ hash_verify_format<F, hash_t>::value };
        static constexpr hash_algorithm ha{ A };
        hash_t hash_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        target_hash_size_t  target_hash_size_;
        algorithm_parameters_t algorithm_parameters_{};

    public:
        explicit hash_verify_fn(size_t target_hash_size = 0) : target_hash_size_{target_hash_size} {}

        explicit hash_verify_fn(H&& hash)
            : hash_{std::forward<H>(hash)} {}

        [[nodiscard]] auto with_blake2b_parameters(sph::blake2b_parameters parameters) const -> hash_verify_fn
            requires (A == sph::hash_algorithm::blake2b)
        {
            auto result{ *this };
            result.algorithm_parameters_ = parameters;
            return result;
        }

        template <hash_range R>
        [[nodiscard]] constexpr auto operator()(R&& range) const
            -> hash_verify_view< std::views::all_t<R>, hash_verify_output<R, hash_t>, ha, hf>
            requires (appended_hash)
        {
            if constexpr (A == sph::hash_algorithm::blake2b)
            {
                return hash_verify_view<std::views::all_t<R>, hash_verify_output<R, hash_t>, ha, hf>(
                    target_hash_size_, std::views::all(std::forward<R>(range)), algorithm_parameters_);
            }
            else
            {
                return hash_verify_view<std::views::all_t<R>, hash_verify_output<R, hash_t>, ha, hf>(
                    target_hash_size_, std::views::all(std::forward<R>(range)));
            }
        }

        template <hash_range R>
        [[nodiscard]] constexpr auto operator()(R&& range) const
            -> hash_verify_view<std::views::all_t<R>, hash_verify_output<R, hash_t>, ha, hf>
            requires (!appended_hash)
        {
            if constexpr (A == sph::hash_algorithm::blake2b)
            {
                return hash_verify_view<std::views::all_t<R>, hash_verify_output<R, hash_t>, ha, hf>(
                    std::views::all(std::forward<R>(range)), std::views::all(hash_), algorithm_parameters_);
            }
            else
            {
                return hash_verify_view<std::views::all_t<R>, hash_verify_output<R, hash_t>, ha, hf>(
                    std::views::all(std::forward<R>(range)), std::views::all(hash_));
            }
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
