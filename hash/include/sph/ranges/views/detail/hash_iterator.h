#pragma once

#include <sph/hash_algorithm.h>
#include <sph/hash_style.h>
#include <sph/ranges/views/detail/get_target_hash_size.h>
#include <sodium/crypto_hash_sha256.h>
#include <sodium/crypto_hash_sha512.h>
#include <sodium/crypto_generichash_blake2b.h>

namespace sph::ranges::views::detail
{
    enum class iterate_style
    {
        no_appended_hash,
        skip_appended_hash
    };



    /**
     * Forward declaration of the zstd_decode_view end-of-sequence
     * sentinel.
     */
    template<std::ranges::viewable_range R, typename T, sph::hash_algorithm A, sph::hash_style S, iterate_style IS>
        requires std::ranges::input_range<R>&& std::is_standard_layout_v<T>&& std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
    struct sentinel;

    /**
     * The iterator used by the hash_view and hash_verify_view
     * providing a view of the hashed stream.
     *
     * @tparam R The type of the range that holds a hashed stream.
     * @tparam T The output type.
     * @tparam A The hash algorithm to use.
     * @tparam S The hash style to use (append to hashed data or separate from hashed data).
     */
    template<std::ranges::viewable_range R, typename T, sph::hash_algorithm A, sph::hash_style S, iterate_style IS>
        requires std::ranges::input_range<R> && std::is_standard_layout_v<T> && std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
    class iterator
    {
    public:
        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using pointer = const bool*;
        using reference = const bool&;
        using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
        using output_type = std::remove_cvref_t<T>;
    private:
        struct empty {};
        struct input_value_with_position { input_type value; size_t position; };
        using input_value_t = std::conditional_t < sizeof(input_type) == 1, empty, input_value_with_position>;
        using state_type_t = std::conditional_t<A == sph::hash_algorithm::blake2b, crypto_generichash_blake2b_state, 
            std::conditional_t<A == sph::hash_algorithm::sha512, crypto_hash_sha512_state, 
            std::conditional_t<A == sph::hash_algorithm::sha256, crypto_hash_sha256_state, void>>>;
        using rolling_buffer_t = std::conditional<IS == iterate_style::no_appended_hash, empty, std::conditional<IS == iterate_style::skip_appended_cache, rolling_buffer<T, A>, void>>;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-attributes"
#endif
        [[no_unique_address]] input_value_t input_{ {}, sizeof(input_type) };
        [[no_unique_address]] rolling_buffer_t rolling_buffer_;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
        std::ranges::const_iterator_t<R> to_hash_current_;
        std::ranges::const_sentinel_t<R> to_hash_end_;
        size_t target_hash_size_;
        state_type_t state_;
        std::array<uint8_t, sph::hash_param<A>::hash_chunk_size> chunk_;
        std::array<uint8_t, sph::hash_param<A>::hash_chunk_size>::iterator chunk_current_ {chunk_.begin()};
        std::array<uint8_t, sph::hash_param<A>::hash_byte_size> hash_;
        output_type value_;
        std::optional<typename std::array<uint8_t, sph::hash_param<A>::hash_size>::iterator> hash_current_;
    public:
        /**
         * Initialize a new instance of the hash_verify_view::iterator
         * class.
         * @param begin The start of the input range to decompress.
         * @param end The end of the input range.
         * @param target_hash_size The size of the hash to create. May be 
         * bigger if the size of the output type causes it to grow. Zero gives
         * largest size available. If the output type is too large, the hash 
         * may not be storable and an exception will be thrown when that point
         * in the output range is reached.
         */
        iterator(std::ranges::const_iterator_t<R> begin, std::ranges::const_sentinel_t<R> end, size_t target_hash_size)
            : to_hash_current_(std::move(begin)), to_hash_end_(std::move(end)), target_hash_size_{ get_target_hash_size<T, A>(target_hash_size) }
        {
            if constexpr (A == sph::hash_algorithm::blake2b)
            {
                crypto_generichash_blake2b_init(&state_, nullptr, 0, target_hash_size_);
            }
            else if constexpr (A == sph::hash_algorithm::sha512)
            {
                crypto_hash_sha512_init(&state_);
            }
            else if constexpr (A == sph::hash_algorithm::sha256)
            {
                crypto_hash_sha256_init(&state_);
            }
            else
            {
                throw std::invalid_argument("Unsupported hash type");
            }

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
            if constexpr (sizeof(input_type) == 1)
            {
                return to_hash_current_ == i.to_hash_current_ 
                    && to_hash_end_ == i.to_hash_end_ 
                    && ((!hash_current_ && !i.hash_current_) 
                        || (hash_current_ && i.hash_current_ && std::distance(hash_.begin(), *hash_current_) == std::distance(i.hash_.begin(), *i.hash_current_)));
            }
            else
            {
                return to_hash_current_ == i.to_hash_current_ 
                    && to_hash_end_ == i.to_hash_end_ 
                    && input_.position == i.input_.position 
                    && ((!hash_current_ && !i.hash_current_) 
                        || (hash_current_ && i.hash_current_ && std::distance(hash_.begin(), *hash_current_) == std::distance(i.hash_.begin(), *i.hash_current_)));
            }
        }

        /**
         * Compare the provided sentinel for equality.
         * @return True if at the end of the decompressed view.
         */
        auto equals(const sentinel<R, T, A, S>&) const noexcept -> bool
        {
            return hash_current_ && std::distance(hash_.begin(), *hash_current_) >= target_hash_size_;
        }

        /**
         * Gets the current decompressed value.
         * @return The current decompressed value.
         */
        auto operator*() const -> output_type
        {
            return value_;
        }

        auto operator==(const iterator& other) const noexcept -> bool { return equals(other); }
        auto operator==(const sentinel<R, T, A, S>& s) const noexcept -> bool { return equals(s); }
        auto operator!=(const iterator& other) const noexcept -> bool { return !equals(other); }
        auto operator!=(const sentinel<R, T, A, S>& s) const noexcept -> bool { return !equals(s); }

    private:
        /**
         * Sets _value to the next decompressed value.
         *
         * Will throw std::invalid_argument for a truncated or otherwise invalid input range.
         */
        void load_value()
        {
            if (hash_current_)
            {
                load_hash_value();
                return;
            }

            if constexpr (A == sph::hash_algorithm::blake2b)
            {
                load_blake2b_value();
            }
            else if constexpr (A == sph::hash_algorithm::sha512)
            {
                load_sha512_value();
            }
            else if constexpr (A == sph::hash_algorithm::sha256)
            {
                load_sha256_value();
            }
            else
            {
                throw std::invalid_argument("Unsupported hash type");
            }
        }

        /**
         * If a hash is available, this gets called to load the next output
         * value.
         */
        auto load_hash_value() -> void
        {
            if constexpr (sizeof(output_type) == 1)
            {
                value_ = static_cast<output_type>(**hash_current_);
                ++*hash_current_;
                return;
            }
            else
            {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-container"
#endif
                std::span<uint8_t, sizeof(output_type)> output_buf{ reinterpret_cast<uint8_t*>(&value_), sizeof(output_type) };
#ifdef __clang__
#pragma clang diagnostic pop
#endif
                if (std::distance(hash_current_, hash_.end()) < output_buf.size())
                {
                    throw std::runtime_error(
                        std::format(
                            "Cannot handle output type size. Not enough hash data to fill the output value. Expected {} bytes, only {}{} hash bytes available.",
                            output_buf.size(),
                            std::distance(*hash_current_, hash_.end()),
                            std::distance(*hash_current_, hash_.end()) < hash_.size() ? std::format(" of {}", hash_.size()) : std::format(""),
                            hash_.size()));
                }

                std::copy_n(*hash_current_, output_buf.size(), output_buf.data());
                *hash_current_ += output_buf.size();
                return;
            }
        }

        auto load_blake2b_value() -> void
        {
            if constexpr (sizeof(output_type) == 1)
            {
                while (true)
                {
                    if (auto [valid_value, value] {next_byte()}; valid_value)
                    {
                        *chunk_current_ = value;
                        if (chunk_current_ == chunk_.end())
                        {
                            crypto_generichash_blake2b_update(&state_, chunk_.data(), chunk_.size());
                            chunk_current_ = chunk_.begin();
                        }

                        value_ = static_cast<output_type>(value);
                        *chunk_current_ = value;
                        ++chunk_current_;
                        if constexpr (S == sph::hash_style::append)
                        {
                            return;
                        }
                    }
                    else
                    {
                        crypto_generichash_blake2b_update(&state_, chunk_.data(), std::distance(chunk_.begin(), chunk_current_));
                        crypto_generichash_blake2b_final(&state_, hash_.data() + hash_.size() - target_hash_size_, target_hash_size_);
                        hash_current_ = hash_.begin() + hash_.size() - target_hash_size_;
                        value_ = static_cast<output_type>(**hash_current_);
                        ++*hash_current_;
                        return;
                    }
                }
            }
            else
            {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-container"
#endif
                std::span<uint8_t, sizeof(output_type)> value_buf{ reinterpret_cast<uint8_t*>(&value_), sizeof(output_type) };
#ifdef __clang__
#pragma clang diagnostic pop
#endif
                auto value_buf_current{ value_buf.begin() };
                while (true)
                {
                    if (auto [valid_value, value] {next_byte()}; valid_value)
                    {
                        *chunk_current_ = value;
                        *value_buf_current = value;

                        if (chunk_current_ == chunk_.end())
                        {
                            crypto_generichash_blake2b_update(&state_, chunk_.data(), chunk_.size());
                            chunk_current_ = chunk_.begin();
                        }

                        if (value_buf_current == value_buf.end())
                        {
                            if constexpr (S == sph::hash_style::append)
                            {
                                return;
                            }
                            else
                            {
                                value_buf_current = value_buf.begin();
                            }
                        }
                    }
                    else
                    {
                        crypto_generichash_blake2b_update(&state_, chunk_.data(), std::distance(chunk_.begin(), chunk_current_));
                        crypto_generichash_blake2b_final(&state_, hash_.data() + hash_.size() - target_hash_size_, target_hash_size_);
                        hash_current_ = hash_.begin() + hash_.size() - target_hash_size_;
                        *value_buf_current++ = **hash_current_++;
                        while (value_buf_current != value_buf.end())
                        {
                            *value_buf_current++ = **hash_current_++;
                        }

                        return;
                    }

                }
            }
        }

        auto load_sha512_value() -> void
        {
            if constexpr (sizeof(output_type) == 1)
            {
                while (true)
                {
                    if (auto [valid_value, value] {next_byte()}; valid_value)
                    {
                        *chunk_current_ = value;
                        if (chunk_current_ == chunk_.end())
                        {
                            crypto_hash_sha512_update(&state_, chunk_.data(), chunk_.size());
                            chunk_current_ = chunk_.begin();
                        }

                        value_ = static_cast<output_type>(value);
                        *chunk_current_ = value;
                        ++chunk_current_;
                        if constexpr (S == sph::hash_style::append)
                        {
                            return;
                        }
                    }
                    else
                    {
                        crypto_hash_sha512_update(&state_, chunk_.data(), std::distance(chunk_.begin(), chunk_current_));
                        crypto_hash_sha512_final(&state_, hash_.data());
                        hash_current_ = hash_.begin();
                        value_ = static_cast<output_type>(**hash_current_);
                        ++*hash_current_;
                        return;
                    }
                }
            }
            else
            {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-container"
#endif
                std::span<uint8_t, sizeof(output_type)> value_buf{ reinterpret_cast<uint8_t*>(&value_), sizeof(output_type) };
#ifdef __clang__
#pragma clang diagnostic pop
#endif
                auto value_buf_current{ value_buf.begin() };
                while (true)
                {
                    if (auto [valid_value, value] {next_byte()}; valid_value)
                    {
                        *chunk_current_ = value;
                        *value_buf_current = value;

                        if (chunk_current_ == chunk_.end())
                        {
                            crypto_hash_sha512_update(&state_, chunk_.data(), chunk_.size());
                            chunk_current_ = chunk_.begin();
                        }

                        if (value_buf_current == value_buf.end())
                        {
                            if constexpr (S == sph::hash_style::append)
                            {
                                return;
                            }
                            else
                            {
                                value_buf_current = value_buf.begin();
                            }
                        }
                    }
                    else
                    {
                        crypto_hash_sha512_update(&state_, chunk_.data(), std::distance(chunk_.begin(), chunk_current_));
                        crypto_hash_sha512_final(&state_, hash_.data());
                        hash_current_ = hash_.begin();
                        *value_buf_current++ = **hash_current_++;
                        while (value_buf_current != value_buf.end())
                        {
                            *value_buf_current++ = **hash_current_++;
                        }

                        return;
                    }
                }
            }
        }

        auto load_sha256_value() -> void
        {
            if constexpr (sizeof(output_type) == 1)
            {
                while (true)
                {
                    if (auto [valid_value, value] {next_byte()}; valid_value)
                    {
                        *chunk_current_ = value;
                        if (chunk_current_ == chunk_.end())
                        {
                            crypto_hash_sha256_update(&state_, chunk_.data(), chunk_.size());
                            chunk_current_ = chunk_.begin();
                        }

                        value_ = static_cast<output_type>(value);
                        *chunk_current_ = value;
                        ++chunk_current_;
                        if constexpr (S == sph::hash_style::append)
                        {
                            return;
                        }
                    }
                    else
                    {
                        crypto_hash_sha256_update(&state_, chunk_.data(), std::distance(chunk_.begin(), chunk_current_));
                        crypto_hash_sha256_final(&state_, hash_.data());
                        hash_current_ = hash_.begin();
                        value_ = static_cast<output_type>(**hash_current_);
                        ++*hash_current_;
                        return;
                    }
                }
            }
            else
            {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-container"
#endif
                std::span<uint8_t, sizeof(output_type)> value_buf{ reinterpret_cast<uint8_t*>(&value_), sizeof(output_type) };
#ifdef __clang__
#pragma clang diagnostic pop
#endif
                auto value_buf_current{ value_buf.begin() };
                while (true)
                {
                    if (auto [valid_value, value] {next_byte()}; valid_value)
                    {
                        *chunk_current_ = value;
                        *value_buf_current = value;

                        if (chunk_current_ == chunk_.end())
                        {
                            crypto_hash_sha256_update(&state_, chunk_.data(), chunk_.size());
                            chunk_current_ = chunk_.begin();
                        }

                        if (value_buf_current == value_buf.end())
                        {
                            if constexpr (S == sph::hash_style::append)
                            {
                                return;
                            }
                            else
                            {
                                value_buf_current = value_buf.begin();
                            }
                        }
                    }
                    else
                    {
                        crypto_hash_sha256_update(&state_, chunk_.data(), std::distance(chunk_.begin(), chunk_current_));
                        crypto_hash_sha256_final(&state_, hash_.data());
                        hash_current_ = hash_.begin();
                        *value_buf_current++ = **hash_current_++;
                        while (value_buf_current != value_buf.end())
                        {
                            *value_buf_current++ = **hash_current_++;
                        }

                        return;
                    }
                }
            }
        }

        auto next_byte_from_input_range() -> std::tuple<bool, uint8_t>
        {
            if constexpr (sizeof(input_type) == 1)
            {
                if (to_hash_current_ == to_hash_end_)
                {
                    return { false, 0 };
                }

                return { true, (*to_hash_current_)++ };
            }
            else
            {
                if (to_hash_current_ == to_hash_end_ && input_.position == sizeof(input_type))
                {
                    return { false, 0 };
                }

                if (input_.position == sizeof(input_type))
                {
                    input_.position = 0;
                    input_.value = *to_hash_current_;
                    ++to_hash_current_;
                }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-container"
#endif
                std::tuple<bool, uint8_t> ret{ true, (std::span<uint8_t, sizeof(input_type)>{ reinterpret_cast<uint8_t*>(&input_.value), sizeof(input_type) })[input_.position] };
#ifdef __clang__
#pragma clang diagnostic pop
#endif
                ++input_.position;
                return ret;
            }
        }

        auto next_byte() -> std::tuple<bool, uint8_t>
        {
            if constexpr (IS == iterate_style::skip_appended_hash)
            {
                if (rolling_buffer_.done())
                {
                    return rolling_buffer_.next();
                }

                while(true)
                {
                    if (auto [valid_value, value] {next_byte_from_input_range()}; valid_value)
                    {
                        auto b{ rolling_buffer_.next(value) };
                        if (b)
                        {
                            return { true, *b };
                        }
                    }
                    else
                    {
                        rolling_buffer_.done(target_hash_size_);
                        return rolling_buffer_.next();
                    }
                }
            }
            else
            {
                return next_byte_from_input_range();
            }
        }
    };

    template<std::ranges::viewable_range R, typename T, sph::hash_algorithm A, sph::hash_style S, iterate_style IS>
        requires std::ranges::input_range<R>&& std::is_standard_layout_v<T>&& std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
    struct sentinel
    {
        auto operator==(const sentinel& /*other*/) const -> bool { return true; }
        auto operator==(const iterator<R, T, A, S>& i) const -> bool { return i.equals(*this); }
        auto operator!=(const sentinel& /*other*/) const -> bool { return false; }
        auto operator!=(const iterator<R, T, A, S>& i) const -> bool { return !i.equals(*this); }
    };
}