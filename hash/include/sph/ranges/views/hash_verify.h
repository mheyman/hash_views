#pragma once
#include <ranges>
#include <sph/hash_algorithm.h>
#include <sph/hash_site.h>
#include <sph/hash_format.h>
#include <sph/ranges/views/detail/single_bool_iterator.h>

namespace sph::ranges::views
{
    namespace detail
    {
        template <typename R>
        concept hash_verify_range = std::ranges::viewable_range<R>
            && std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
            && std::is_trivially_copyable_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
            && !std::is_pointer_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
            && !std::is_reference_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>;

        template <typename R>
        concept hash_verify_single_byte_range = hash_verify_range<R> &&
            (sizeof(std::remove_cvref_t<std::ranges::range_value_t<R>>) == 1);

        template <typename R>
        concept hash_verify_multi_byte_range = hash_verify_range<R> &&
            (sizeof(std::remove_cvref_t<std::ranges::range_value_t<R>>) > 1);

        /**
         * Concept for a range or std::nullopt_t.
         * @tparam H The type of the range.
         */
        template <typename H>
        concept hash_verify_fn_hash_range = hash_verify_range<H> || std::is_same_v<H, std::nullopt_t>;

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
        template<hash_verify_fn_hash_format F, std::ranges::viewable_range R>
        struct hash_verify_format
        {
            static constexpr sph::hash_format value = std::conditional_t<std::is_same_v<F, std::nullopt_t>, std::conditional_t<sizeof(std::remove_cvref_t<std::ranges::range_value_t<R>>) == 1, hash_verify_format_raw, hash_verify_format_padded>, F>();
        };
        
        /**
         * Provides view of hash of the input range.
         * @tparam R The type of the range that holds a hashed stream.
         * @tparam A The type of the hash to do.
         */
        template<hash_verify_range R, sph::hash_algorithm A, sph::hash_format F>
            requires std::ranges::input_range<R> && std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
        class hash_verify_view : public std::ranges::view_interface<hash_verify_view<R, A, F>> {
            bool verify_ok_{ false };

            using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
            using input_append_iterator = detail::hash_iterator<R, input_type, A, F, sph::hash_site::append, end_of_input::skip_appended_hash>;
            using input_append_sentinel = detail::hash_sentinel<R, input_type, A, F, sph::hash_site::append, end_of_input::skip_appended_hash>;
            using input_separate_iterator = detail::hash_iterator<R, input_type, A, F, sph::hash_site::separate, end_of_input::no_appended_hash>;
            using input_separate_sentinel = detail::hash_sentinel<R, input_type, A, F, sph::hash_site::separate, end_of_input::no_appended_hash>;
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
             * @param target_hash_size the size (in bytes) of the hash.
             * @param input the range to verify.
             * @param hash the hash to compare against.
             */
            template<hash_verify_range H>
            hash_verify_view(size_t target_hash_size, R&& input, H&& hash)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
                : verify_ok_{verify(target_hash_size, std::forward<R>(input), std::forward<H>(hash))}
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
            ~hash_verify_view() noexcept = default;
            auto operator=(hash_verify_view const&) -> hash_verify_view& = default;
            auto operator=(hash_verify_view&&o) noexcept -> hash_verify_view& = default;


            [[nodiscard]] auto begin() -> iterator { return iterator(verify_ok_); }
            [[nodiscard]] auto begin() const -> iterator { return iterator(verify_ok_); }

            [[nodiscard]] auto end() -> sentinel { return sentinel{}; }
            [[nodiscard]] auto end() const -> sentinel { return sentinel{}; }
        private:

            /**
             * Convert hash into a vector of uint8_t.
             */
            template<hash_verify_single_byte_range H>
            static constexpr auto hash_to_byte_vector(H&& hash) -> std::vector<uint8_t>
            {
                using value_t = std::remove_cvref_t<std::ranges::range_value_t<H>>;
                return std::forward<H>(hash)
                    | std::views::transform([](value_t v) -> uint8_t
                        {
                            return static_cast<uint8_t>(v);
                        })
                    | std::ranges::to<std::vector>();
            }

            template<hash_verify_multi_byte_range H>
            static constexpr auto hash_to_byte_vector(H&& hash) -> std::vector<uint8_t>
            {
                using value_t = std::remove_cvref_t<std::ranges::range_value_t<H>>;
                return std::forward<H>(hash)
                    | std::views::transform([](value_t v) -> std::array<uint8_t, sizeof(value_t)>
                        {
                            std::array<uint8_t, sizeof(value_t)> ret{};
                            std::copy_n(reinterpret_cast<uint8_t*>(&v), sizeof(value_t), ret.data());
                            return ret;
                        })
                    | std::views::join
                    | std::ranges::to<std::vector>();
            }

            template<hash_verify_range H>
            static auto verify(size_t target_hash_size, R&& input, H&& hash) -> bool
            {
                std::vector<uint8_t> provided_hash{ hash_to_byte_vector(std::forward<H>(hash)) };
                R to_hash{ std::move(input) };
                std::vector<uint8_t> hash_result
                {
                    hash_to_byte_vector(
                        std::ranges::subrange(input_separate_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size), input_separate_sentinel{}))
                };
                if (provided_hash.size() != hash_result.size())
                {
                    fmt::print("H size mismatch: expected {} bytes, calculated {} bytes.\n  expected {},\n       got {}\n", provided_hash.size(), hash_result.size(), fmt::join(provided_hash | std::views::transform([](auto x) -> std::string { return fmt::format("{:02X}", static_cast<uint8_t>(x)); }), " "),
                        fmt::join(hash_result | std::views::transform([](auto x) -> std::string { return fmt::format("{:02X}", static_cast<uint8_t>(x)); }), " "));
                    return false;
                }

                bool const ret{ std::ranges::all_of(std::views::zip(provided_hash, hash_result), [](auto&& pair)
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
                std::vector<uint8_t> hash_result{ hash_to_byte_vector(std::ranges::subrange(hasher, input_append_sentinel{})) };
                std::vector<uint8_t> appended_hash{ hash_to_byte_vector(hasher.hash()) }; // available after iteration complete
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
            struct empty {};
            using hash_t = std::conditional_t<appended_hash, empty, H>;
            hash_t hash_;
            size_t target_hash_size_;

        public:
            explicit hash_verify_fn(size_t target_hash_size = 0) : target_hash_size_{target_hash_size} {}

            explicit hash_verify_fn(H&& hash, size_t target_hash_size = 0)
                : hash_{std::move(hash)}, target_hash_size_{target_hash_size} {}

            template <std::ranges::viewable_range R>
            [[nodiscard]] constexpr auto operator()(R&& range) const
                -> hash_verify_view<std::views::all_t<R>, A, hash_verify_format<F, R>::value>
                requires appended_hash
            {
                return hash_verify_view<std::views::all_t<R>, A, hash_verify_format<F, R>::value>(
                    target_hash_size_, std::views::all(std::forward<R>(range)));
            }

            template <std::ranges::viewable_range R>
            [[nodiscard]] constexpr auto operator()(R&& range) const
                -> hash_verify_view<std::views::all_t<R>, A, hash_verify_format<F, R>::value>
            requires (!appended_hash)
            {
                return hash_verify_view<std::views::all_t<R>, A, hash_verify_format<F, R>::value>(
                    target_hash_size_, std::views::all(std::forward<R>(range)), std::views::all(hash_));
            }
        };

    }
}

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
     * @param target_hash_size the minimum size in bytes of the hash created.
     * @param hash the range holding the hash to compare against.
     * @return A functor that takes a hashed+hash range or separate hashed and hash ranges and returns a view of the verification status.
     */
    template<sph::hash_algorithm A, sph::hash_format F, sph::ranges::views::detail::hash_verify_range H>
    auto hash_verify(H&& hash, size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, H>{std::forward<H>(hash), target_hash_size};
    }

    template<sph::hash_format F, sph::hash_algorithm A, sph::ranges::views::detail::hash_verify_range H>
    auto hash_verify(H&& hash, size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, ranges::views::detail::hash_verify_format_t<F>, H>{std::forward<H>(hash), target_hash_size};
    }

    template<sph::hash_algorithm A, sph::ranges::views::detail::hash_verify_range H>
    auto hash_verify(H&& hash, size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<A, std::nullopt_t, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, std::nullopt_t, H>{std::forward<H>(hash), target_hash_size};
    }

    template<sph::hash_format F, sph::ranges::views::detail::hash_verify_range H>
    auto hash_verify(H&& hash, size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, ranges::views::detail::hash_verify_format_t<F>, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, ranges::views::detail::hash_verify_format_t<F>, H>{std::forward<H>(hash), target_hash_size};
    }

    template<sph::ranges::views::detail::hash_verify_range H>
    auto hash_verify(H&& hash, size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, std::nullopt_t, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<sph::hash_algorithm::blake2b, std::nullopt_t, H>{std::forward<H>(hash), target_hash_size};
    }

}

