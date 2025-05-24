#pragma once
#include <ranges>
#include <sph/hash_algorithm.h>

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
         * @tparam T The type of the hash to do.
         */
        template<std::ranges::viewable_range R, std::ranges::viewable_range H, sph::hash_algorithm A, sph::hash_style S>
            requires std::ranges::input_range<R> && std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
                  && std::ranges::input_range<H> && std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
        class hash_verify_view : public std::ranges::view_interface<hash_verify_view<R, H, A, S>> {
            bool verify_ok_{ false };

            using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
            static constexpr sph::ranges::views::detail::iterate_style iterate_style =
                S == sph::hash_style::append || S == sph::hash_style::append_padded
                    ? sph::ranges::views::detail::iterate_style::skip_appended_hash 
                    : sph::ranges::views::detail::iterate_style::no_appended_hash;
            using input_iterator = detail::hash_iterator<R, input_type, A, S, iterate_style>;
            using input_sentinel = detail::hash_sentinel<R, input_type, A, S, iterate_style>;
            class bool_sentinel;
            class bool_iterator
            {
                bool value_;
                bool done_{ false };
            public:
                using iterator_concept = std::input_iterator_tag;
                using iterator_category = std::input_iterator_tag;
                using difference_type = std::ptrdiff_t;
                using pointer = const bool*;
                using reference = const bool&;
                explicit bool_iterator(bool value) : value_{ value } {}
                auto operator++() -> bool_iterator&
                {
                    done_ = true;
                    return *this;
                }
                auto operator++(int) -> bool_iterator&
                {
                    done_ = true;
                    return *this;
                }
                auto operator*() const -> bool { return value_; }
                auto operator==(const bool_iterator& other) const noexcept -> bool { return done_ == other.done_; }
                auto operator==(const bool_sentinel& ) const noexcept -> bool { return done_; }
                auto operator!=(const bool_iterator& other) const noexcept -> bool { return done_ == other.done_; }
                auto operator!=(const bool_sentinel& ) const noexcept -> bool { return !done_; }
            };

            class bool_sentinel
            {
                auto operator==(const bool_sentinel& /*other*/) const -> bool { return true; }
                auto operator==(const bool_iterator& i) const -> bool { return i == *this; }
                auto operator!=(const bool_sentinel& /*other*/) const -> bool { return false; }
                auto operator!=(const bool_iterator& i) const -> bool { return i != *this; }
            };
        public:
            using iterator = bool_iterator;
            using sentinel = bool_sentinel;

            /**
             * Initialize a new instance of the hash_verify_view class.
             *
             * Provides a begin() iterator and end() sentinel over the
             * verified view of the given input range.
             *
             * @param input the range to verify.
             */
            template<sph::hash_style HS = S>
                requires (HS == sph::hash_style::separate)
            hash_verify_view(size_t target_hash_size, R&& input, H&& hash)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
                : verify_ok_{verify(target_hash_size, std::forward<R>(input), std::forward<H>(hash))}
            {}

            template<sph::hash_style HS = S>
                requires (HS == sph::hash_style::append || S == sph::hash_style::append_padded)
            hash_verify_view(size_t target_hash_size, R&& input)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
                : verify_ok_ { verify(target_hash_size, std::forward<R>(input)) }
            {}

            hash_verify_view(hash_verify_view const&) = default;
            hash_verify_view(hash_verify_view&&) = default;
            ~hash_verify_view() noexcept = default;
            auto operator=(hash_verify_view const&) -> hash_verify_view& = default;
            auto operator=(hash_verify_view&&o) noexcept -> hash_verify_view& = default;


            iterator begin() const { return bool_iterator(verify_ok_); }

            sentinel end() const { return sentinel{}; }
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
                std::vector<uint8_t> provided_hash{ hash_to_byte_vector(std::forward<H>(hash)) };
                std::vector<uint8_t> hash_result
                {
                    hash_to_byte_vector(
                        std::ranges::subrange(input_iterator(std::ranges::begin(input), std::ranges::end(input), target_hash_size), input_sentinel{}))
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
                auto hasher { input_iterator(std::ranges::begin(input), std::ranges::end(input), target_hash_size) };
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

        template<std::ranges::viewable_range R, sph::hash_algorithm A = sph::hash_algorithm::blake2b, sph::hash_style S = sph::hash_style::append>
        hash_verify_view(R&&) -> hash_verify_view<R, R, A, S>;

        template<std::ranges::viewable_range R, std::ranges::viewable_range H, sph::hash_algorithm A = sph::hash_algorithm::blake2b, sph::hash_style S = sph::hash_style::separate>
        hash_verify_view(R&&, H&&) -> hash_verify_view<R, H, A, S>;

        /**
         * Functor that, given a hashed+hash range or hashed range and a 
         * separate hash range, provides a range of a single boolean indicating
         * if the hash verified correctly or not.
         * @tparam A The hash algorithm.
         * @tparam S The hash algorithm.
         */
        template <sph::hash_algorithm A>
        class hash_verify_fn : public std::ranges::range_adaptor_closure<hash_verify_fn<A>>
        {
            size_t target_hash_size_;
        public:
            explicit hash_verify_fn(size_t target_hash_size = 0) : target_hash_size_{ target_hash_size } {}


            // default to append style if sizeof range value is 1, else append_padded.
            template <single_byte_viewable_range R, sph::hash_style S = sph::hash_style::append>
            [[nodiscard]] constexpr auto operator()(R && range) const -> hash_verify_view<std::views::all_t<R>, std::views::all_t<R>, A, S>
            {
                static_assert(S == sph::hash_style::append || S == sph::hash_style::append_padded, "Invalid hash style for appended hash.");
                return hash_verify_view<std::views::all_t<R>, std::views::all_t<R>, A, sph::hash_style::append>(target_hash_size_, std::views::all(std::forward<R>(range)));
            }

            // default to append style if sizeof range value is 1, else append_padded.
            template <multi_byte_viewable_range R, sph::hash_style S = sph::hash_style::append_padded>
            [[nodiscard]] constexpr auto operator()(R && range) const -> hash_verify_view<std::views::all_t<R>, std::views::all_t<R>, A, S>
            {
                static_assert(S == sph::hash_style::append || S == sph::hash_style::append_padded, "Invalid hash style for appended hash.");
                return hash_verify_view<std::views::all_t<R>, std::views::all_t<R>, A, sph::hash_style::append>(target_hash_size_, std::views::all(std::forward<R>(range)));
            }

            template <std::ranges::viewable_range R, std::ranges::viewable_range H, sph::hash_style S = sph::hash_style::separate>
            [[nodiscard]] constexpr auto operator()(R&& hashed_range, H&& hash_range) const -> hash_verify_view<std::views::all_t<R>, std::views::all_t<H>, A, S>
            {
                static_assert(S == sph::hash_style::separate || S == sph::hash_style::separate_padded, "Invalid hash style for non-appended hash.");
                return hash_verify_view<std::views::all_t<R>, std::views::all_t<H>, A, sph::hash_style::separate>(
                    target_hash_size_, std::views::all(std::forward<R>(hashed_range)), std::views::all(std::forward<H>(hash_range)));
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
	template<sph::hash_algorithm A>
    auto hash_verify(size_t target_hash_size = 0) -> sph::ranges::views::detail::hash_verify_fn<A>
    {
        return sph::ranges::views::detail::hash_verify_fn<A>{target_hash_size};
    }
}
