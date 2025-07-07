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
        concept single_byte_viewable_range = std::ranges::viewable_range<R> &&
            (sizeof(std::remove_cvref_t<std::ranges::range_value_t<R>>) == 1);

        template <typename R>
        concept multi_byte_viewable_range = std::ranges::viewable_range<R> &&
            (sizeof(std::remove_cvref_t<std::ranges::range_value_t<R>>) > 1);

        /**
         * Provides view of hash of the input range.
         * @tparam R The type of the range that holds a hashed stream.
         * @tparam H The type of the range that holds a hash stream.
         * @tparam A The type of the hash to do.
         * @tparam S The style of the hash, append, append_padded, separate, or separate_padded.
         */
        template<std::ranges::viewable_range R, std::ranges::viewable_range H, sph::hash_algorithm A, sph::hash_site S, sph::hash_format F>
            requires std::ranges::input_range<R> && std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
                  && std::ranges::input_range<H> && std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
        class hash_verify_view : public std::ranges::view_interface<hash_verify_view<R, H, A, S, F>> {
            bool verify_ok_{ false };

            using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
            static constexpr sph::ranges::views::detail::iterate_style iterate_style =
                S == sph::hash_site::append 
                    ? sph::ranges::views::detail::iterate_style::skip_appended_hash 
                    : sph::ranges::views::detail::iterate_style::no_appended_hash;
            using input_iterator = detail::hash_iterator<R, input_type, A, S, iterate_style>;
            using input_sentinel = detail::hash_sentinel<R, input_type, A, S, iterate_style>;
        public:
            using iterator = single_bool_iterator;
            using sentinel = single_bool_sentinel;

            /**
             * Initialize a new instance of the hash_verify_view class.
             *
             * Provides a begin() iterator and end() sentinel over the
             * verified view of the given input range.
             *
             * @param target_hash_size the size (in bytes) of the hash.
             * @param input the range to verify.
             * @param hash the hash to compare against.
             */
            template<sph::hash_site HS = S>
                requires (HS == sph::hash_site::separate)
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
            template<sph::hash_site HS = S>
                requires (HS == sph::hash_site::append)
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
            template<typename Hash>
            static constexpr auto hash_to_byte_vector(Hash&& hash) -> std::vector<uint8_t>
            {
                using value_t = std::remove_cvref_t<std::ranges::range_value_t<Hash>>;
                return std::forward<Hash>(hash)
                    | std::views::transform([](value_t v) -> std::array<uint8_t, sizeof(value_t)>
                        {
                            std::array<uint8_t, sizeof(value_t)> ret{};
                            std::copy_n(reinterpret_cast<uint8_t*>(&v), sizeof(value_t), ret.data());
                            return ret;
                        })
                    | std::views::join
                    | std::ranges::to<std::vector>();
            }

            static auto verify(size_t target_hash_size, R&& input, H&& hash) -> bool
            {
                std::vector<uint8_t> provided_hash{ hash_to_byte_vector(std::move(hash)) };
                R to_hash{ std::move(input) };
                std::vector<uint8_t> hash_result
                {
                    hash_to_byte_vector(
                        std::ranges::subrange(input_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size), input_sentinel{}))
                };
                if (provided_hash.size() != hash_result.size())
                {
                    return false;
                }

                return std::ranges::all_of(std::views::zip(provided_hash, hash_result), [](auto&& pair)
                    {
                        return std::get<0>(pair) == std::get<1>(pair);
                    });
            }

            static auto verify(size_t target_hash_size, R&& input) -> bool
            {
                R to_hash{ std::move(input) };
                auto hasher { input_iterator(std::ranges::begin(to_hash), std::ranges::end(to_hash), target_hash_size) };
                std::vector<uint8_t> hash_result{ hash_to_byte_vector(std::ranges::subrange(hasher, input_sentinel{})) };
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

        template<std::ranges::viewable_range R, sph::hash_algorithm A = sph::hash_algorithm::blake2b, sph::hash_format F = sph::hash_format::raw>
        hash_verify_view(R&&) -> hash_verify_view<R, R, A, sph::hash_site::append, F>;

        template<std::ranges::viewable_range R, std::ranges::viewable_range H, sph::hash_algorithm A = sph::hash_algorithm::blake2b, sph::hash_format F = sph::hash_format::raw>
        hash_verify_view(R&&, H&&) -> hash_verify_view<R, H, A, sph::hash_site::separate, F>;

        /**
         * Functor that, given a hashed+hash range or hashed range and a 
         * separate hash range, provides a range of a single boolean indicating
         * if the hash verified correctly or not.
         * @tparam A The hash algorithm.
         * @tparam H The hash data range type to verify against.
         */
        template <sph::hash_algorithm A, sph::hash_format F = sph::hash_format::raw, sph::hash_site S = sph::hash_site::append, std::ranges::viewable_range H = std::nullopt_t>
        class hash_verify_fn : public std::ranges::range_adaptor_closure<hash_verify_fn<A, F, S, H>>
        {
            std::optional<std::remove_reference_t<H>> hash_;
            size_t target_hash_size_;

        public:
            explicit hash_verify_fn(size_t target_hash_size = 0) : target_hash_size_{target_hash_size} {}

            explicit hash_verify_fn(H&& hash, size_t target_hash_size = 0)
                : hash_{std::move(hash)}, target_hash_size_{target_hash_size} {}

            template <std::ranges::viewable_range R>
            [[nodiscard]] constexpr auto operator()(R&& range) const
                -> hash_verify_view<std::views::all_t<R>, std::views::all_t<H>, A, S, F>
            {
                if constexpr (std::is_same_v<H, std::nullopt_t>)
                {
                    static_assert(S == sph::hash_site::append, "Require sph::hash_site::append for appended hash.");
                    return hash_verify_view<std::views::all_t<R>, std::views::all_t<R>, A, sph::hash_site::append, F>(
                        target_hash_size_, std::views::all(std::forward<R>(range)));
                }
                else
                {
                    static_assert(S == sph::hash_site::separate, "Require sph::hash_site::separate for non-appended hash.");
                    return hash_verify_view<std::views::all_t<R>, std::views::all_t<H>, A, S, F>(
                        target_hash_size_, std::views::all(std::forward<R>(range)), std::views::all(*hash_));
                }
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
	template<sph::hash_algorithm A, sph::hash_format F = sph::hash_format::raw>
    auto hash_verify(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<A, F, sph::hash_site::append>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, F, sph::hash_site::append>{target_hash_size};
    }

    /**
     * A range adaptor that represents view of an underlying single-element sequence of the hash verification.
     *
     * @tparam A The hash algorithm
     * @param hash The hash verified against.
     * @param target_hash_size the minimum size in bytes of the hash created.
     * @return A functor that takes a hashed+hash range or separate hashed and hash ranges and returns a view of the verification status.
     */
    template<sph::hash_algorithm A, sph::hash_format F = sph::hash_format::raw, std::ranges::viewable_range H>
    auto hash_verify(H&& hash, size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<A, F, sph::hash_site::separate, H>
    {
        return sph::ranges::views::detail::hash_verify_fn<A, F, sph::hash_site::separate, H>{std::forward<H>(hash), target_hash_size};
    }

}

