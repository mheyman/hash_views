#pragma once
#include <format>
#include <ranges>
#include <stdexcept>
#include <sodium/crypto_generichash_blake2b.h>
#include <sodium/crypto_hash_sha256.h>
#include <sodium/crypto_hash_sha512.h>
#include <sph/hash_algorithm.h>

namespace sph::ranges::views
{
    namespace detail
    {
        /**
         * Provides view of hash of the input range.
         * @tparam R The type of the range that holds a hashed stream.
         * @tparam H The type of the range that holds a hash stream.
         * @tparam T The type of the hash to do.
         */
        template<std::ranges::viewable_range R, std::ranges::viewable_range H, sph::views::hash_type T>
            requires std::ranges::input_range<R> && std::is_standard_layout_v<T>&& std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
        class hash_verify_view : public std::ranges::view_interface<hash_verify_view<R, T>> {
            R input_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
            R hash_;
        public:
            /**
             * Initialize a new instance of the hash_verify_view class.
             *
             * Provides a begin() iterator and end() sentinel over the
             * verified view of the given input range.
             *
             * @param input the range to verify.
             */
            zstd_decode_view(R&& input, H&& hash)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
                : input_(std::forward<R>(input)), hash_{hash} {}

            hash_verify_view(zstd_decode_view const&) = default;
            hash_verify_view(zstd_decode_view&&) = default;
            ~hash_verify_view() noexcept = default;
            auto operator=(hash_verify_view const&) -> hash_verify_view& = default;
            auto operator=(hash_verify_view&&o) noexcept -> hash_verify_view& = default;


            iterator begin() const { return iterator(window_log_max_, std::ranges::begin(input_), std::ranges::end(input_)); }

            sentinel end() const { return sentinel{}; }
        };

        template<std::ranges::viewable_range R, typename T = uint8_t>
        zstd_decode_view(R&&) -> zstd_decode_view<R, T>;

        /**
         * Functor that, given a zstd compressed range, provides a decompressed view of that range.
         * @tparam T The type to decompress into.
         */
        template <typename T>
        class zstd_decode_fn : public std::ranges::range_adaptor_closure<zstd_decode_fn<T>>
        {
            int window_log_max_;
        public:
            explicit zstd_decode_fn(int window_log_max = 0) : window_log_max_{ window_log_max } {}
            template <std::ranges::viewable_range R>
            [[nodiscard]] constexpr auto operator()(R&& range) const -> zstd_decode_view<std::views::all_t<R>, T>
            {
                return zstd_decode_view<std::views::all_t<R>, T>(window_log_max_, std::views::all(std::forward<R>(range)));
            }
        };
    }
}

namespace sph::views
{
	/**
     * A range adaptor that represents view of an underlying sequence after applying zstd decompression to each element.
     *
     * Will fail to decompress and throw a std::invalid_argument if the provided range does not represent a valid zstd compressed stream.
     * 
     * @tparam T The type to decompress into. Should probably match, but
     * doesn't have to match, the type that was compressed from.
     * @param window_log_max Size limit (in powers of 2) beyond which the
     * decompressor will refuse to allocate a memory buffer in order to protect
     * the host; zero for default. Valid values (typically): 11 through 30
     * (32-bit), 11 through 31 (64-bit). Out of range values will be clamped.
     * @return A functor that takes a zstd compressed range and returns a view of the decompressed information.
	 */
	template<typename T = uint8_t>
    auto zstd_decode(int window_log_max = 0) -> sph::ranges::views::detail::zstd_decode_fn<T>
    {
        return sph::ranges::views::detail::zstd_decode_fn<T>{window_log_max};
    }
}
