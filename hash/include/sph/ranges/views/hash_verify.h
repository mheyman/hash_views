#pragma once
#include <format>
#include <ranges>
#include <stdexcept>
#include <sodium/crypto_generic_blake2b.h>
#include <sodium/crypto_hash_sha256.h>
#include <sodium/crypto_hash_sha512.h>
#include <sph/ranges/views/hash_type.h>

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

            /**
             * Forward declaration of the zstd_decode_view end-of-sequence
             * sentinel.
             */
            struct sentinel;

            /**
             * The iterator for the zstd_decode_view providing a view of the
             * decompressed stream.
             *
             * This uses the zstd_decompressor class to do the work.
             */
            class iterator
            {
            public:
                using iterator_concept = std::input_iterator_tag;
                using iterator_category = std::input_iterator_tag;
                using difference_type = std::ptrdiff_t;
                using pointer = const bool*;
                using reference = const bool&;
				using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
            private:
                using state_t = std::conditional_t<T == sph::views::hash_type::BLAKE2B, crypto_generichash_blake2b_state, std::conditional_t<T == sph::views::hash_type::SHA512, crypto_hash_sha512_state, crypto_hash_sha256_state>>;
                using buf_t = std::conditional_t<T == sph::views::hash_type::BLAKE2B, std::array<uint8_t, 256>, std::conditional_t<T == sph::views::hash_type::SHA512, std::array<uint8_t, 128>, std::array<uint8_t, 64>>;
                size_t hash_size_{ 0 };
                std::ranges::const_iterator_t<R> hashed_current_;
                size_t current_pos_{ 0 };
                std::ranges::const_sentinel_t<R> hashed_end_;
                std::ranges::const_iterator_t<H> hash_current_;
                std::ranges::const_sentinel_t<H> hash_end_;
                bool value_{ true };
            public:
                /**
                 * Initialize a new instance of the hash_verify_view::iterator
                 * class.
                 * @param begin The start of the input range to decompress.
                 * @param end The end of the input range.
                 */
                iterator(std::ranges::const_iterator_t<R> hashed_begin, std::ranges::const_sentinel_t<R> hashed_end, std::ranges::const_iterator_t<H> hash_begin, std::ranges::const_sentinel_t<H> hash_end, size_t hash_size)
                    : hashed_current_(std::move(hashed_begin)), hashed_end_(std::move(hashed_end)), hash_current_(std::move(hash_begin)), hash_end_(std::move(hash_end)), hash_size_{ hash_size }
                {
                    load_value();
                }

                /**
                 * Increment the iterator.
                 * @return The pre-incremented iterator value. This iterator is
                 * a copy and cannot be used for additional compressing.
                 */
                auto operator++(int) -> iterator&
                {
                    auto ret{ *this };
                    load_next_value();
                    return ret;
                }

                /**
                 * Increment the iterator.
                 * @return The incremented iterator value.
                 */
                auto operator++() -> iterator&
                {
                    load_next_value();
                    return *this;
                }

                /**
                 * Compare the provided iterator for equality.
                 * @param i The iterator to compare against.
                 * @return True if the provided iterator is the same as this one.
                 */
                auto equals(const iterator& i) const noexcept -> bool
                {
                    return true;
                }

                /**
                 * Compare the provided sentinel for equality.
                 * @return True if at the end of the decompressed view.
                 */
                auto equals(const sentinel&) const noexcept -> bool
                {
                    return true;
                }

                /**
                 * Gets the current decompressed value.
                 * @return The current decompressed value.
                 */
                auto operator*() const -> bool
                {
	                return value_;
                }

                auto operator==(const iterator& other) const noexcept -> bool { return equals(other); }
                auto operator==(const sentinel&s) const noexcept -> bool { return equals(s); }
                auto operator!=(const iterator& other) const noexcept -> bool { return !equals(other); }
                auto operator!=(const sentinel&s) const noexcept -> bool { return !equals(s); }

            private:
                /**
                 * Sets _value to the next decompressed value.
                 *
                 * Will throw std::invalid_argument for a truncated or otherwise invalid input range.
                 */
                void load_value()
                {
                    if constexpr (T == sph::views::hash_type::BLAKE2B)
                    {
                        load_blake2b_value();
                    }
                    else if constexpr (T == sph::views::hash_type::SHA512)
                    {
                        load_sha512_value();
                    }
                    else if constexpr (T == sph::views::hash_type::SHA256)
                    {
                        load_sha256_value();
                    }
                    else
                    {
                        throw std::invalid_argument("Unsupported hash type");
                    }
                }

                auto load_blake2b_value() -> void
                {
                    crypto_generichash_blake2b_state state;
                    std::array<uint8_t, 256> buf;
                    crypto_generichash_blake2b_init(&state, nullptr, 0, hash_size_);
                    auto buf_current{ buf.begin() };
                    size_t count{ 0 };
                    while (hashed_current_ != hashed_end_)
                    {
                        if (buf_current == buf.end())
                        {
                            crypto_generichash_blake2b_update(&state, buf.data(), count);
                            buf_current = buf.begin();
                            count = 0;
                        }

                        *buf_current = *hashed_current_;
                        ++buf_current;
                        ++hashed_current_;
                        ++count;
                    }

                    crypto_generichash_blake2b_update(&state, buf.data(), count);
                    std::array<uint8_t, crypto_generichash_blake2b_BYTES_MAX> hash;
                    crypto_generichash_blake2b_final(&state, hash.data(), hash.size());
                    auto verify_current{ hash.begin() };
                    while (hash_current_ != hash_end_)
                    {
                        if (verify_current == hash.end())
                        {
                            // truncating is okay but here the provided 
                            // hash is longer than blake2b can provide 
                            // without some extension mechanism not covered
                            // here. Note, Blake2b mixes the digest 
                            // length into the hash so this code has very 
                            // very very little chance of executing because
                            // the hash value should fail to match the 
                            // verify value long before hitting this code.
                            value_ = false;
                            return;
                        }

                        if (*hash_current_ != *verify_current)
                        {
                            value_ = false;
                            return;
                        }

                        ++hash_current_;
                        ++verify_current;
                    }
                }

                auto load_sha512_value() -> void
                {
                    crypto_hash_sha512_state state;
                    std::array<uint8_t, 128> buf;
                    crypto_hash_sha512_init(&state);
                    auto buf_current{ buf.begin() };
                    size_t count{ 0 };
                    while (hashed_current_ != hashed_end_)
                    {
                        if (buf_current == buf.end())
                        {
                            crypto_hash_sha512_update(&state, buf.data(), count);
                            buf_current = buf.begin();
                            count = 0;
                        }

                        *buf_current = *hashed_current_;
                        ++buf_current;
                        ++hashed_current_;
                        ++count;
                    }

                    crypto_hash_sha512_update(&state, buf.data(), count);
                    std::array<uint8_t, crypto_hash_sha512_BYTES> hash;
                    crypto_hash_sha512_final(&state, hash.data());
                    auto verify_current{ hash.begin() };
                    while (hash_current_ != hash_end_)
                    {
                        if (verify_current == hash.end())
                        {
                            // truncating is okay but here the provided 
                            // hash is longer than sha512 can provide 
                            // without some extension mechanism not covered
                            // here.
                            value_ = false;
                            return;
                        }

                        if (*hash_current_ != *verify_current)
                        {
                            value_ = false;
                            return;
                        }

                        ++hash_current_;
                        ++verify_current;
                    }
                }

                auto load_sha256_value() -> void
                {
                    crypto_hash_sha256_state state;
                    std::array<uint8_t, 64> buf;
                    crypto_hash_sha512_init(&state);
                    auto buf_current{ buf.begin() };
                    size_t count{ 0 };
                    while (hashed_current_ != hashed_end_)
                    {
                        if (buf_current == buf.end())
                        {
                            crypto_hash_sha256_update(&state, buf.data(), count);
                            buf_current = buf.begin();
                            count = 0;
                        }

                        *buf_current = *hashed_current_;
                        ++buf_current;
                        ++hashed_current_;
                        ++count;
                    }

                    crypto_hash_sha256_update(&state, buf.data(), count);
                    std::array<uint8_t, crypto_hash_sha256_BYTES> hash;
                    crypto_hash_sha256_final(&state, hash.data());
                    auto verify_current{ hash.begin() };
                    while (hash_current_ != hash_end_)
                    {
                        if (verify_current == hash.end())
                        {
                            // truncating is okay but here the provided 
                            // hash is longer than sha256 can provide 
                            // without some extension mechanism not covered
                            // here.
                            value_ = false;
                            return;
                        }

                        if (*hash_current_ != *verify_current)
                        {
                            value_ = false;
                            return;
                        }

                        ++hash_current_;
                        ++verify_current;
                    }
                }
            };

            struct sentinel
        	{
                auto operator==(const sentinel& /*other*/) const -> bool { return true; }
                auto operator==(const iterator& i) const -> bool { return i.equals(*this); }
                auto operator!=(const sentinel& /*other*/) const -> bool { return false; }
                auto operator!=(const iterator& i) const -> bool { return !i.equals(*this); }
            };

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
