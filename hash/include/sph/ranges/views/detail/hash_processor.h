#pragma once
#include <type_traits>
#include <sph/hash_site.h>
#include <sph/hash_format.h>
#include <sph/ranges/views/detail/padded_hash.h>
#include <sph/ranges/views/detail/process_util.h>

namespace sph::ranges::views::detail
{
    /**
     * Process input data and return a hash.
     *
     * The process method of this class takes a function that returns a byte
     * value and a boolean indicating whether the byte is valid or the input
     * is complete. The function is called repeatedly until it returns false,
     * at which point the hash is finalized.
     *
     * Depending on the hash style, the process method may return the passed in
     * values or just the final hash value.
     *
     * The padding, if any, is the standard bit-padding of a single 1 bit followed by
     * as many 0 bits as needed to fill the chunk size.
     *
     * @tparam O The output type. Process returns one of these each time it is called.
     * @tparam S The hash site. Append, or separate.
     * @tparam F The hash format. padded, or raw.
     * @tparam H The hash. Sha256, sha512, Blake2b.
     */
    template<typename O, sph::hash_site S, sph::hash_format F, basic_hash H>
    class hash_processor  // NOLINT(clang-diagnostic-padded)
    {
        static constexpr bool return_inputs{ S == sph::hash_site::append};
        static constexpr bool single_byte{ sizeof(O) == 1 };
        static constexpr bool pad_hash{ F == sph::hash_format::padded};
        using hash_t = std::conditional_t<pad_hash, padded_hash<O, H>, H>;
        using hash_begin_t = decltype(std::declval<hash_t>().hash().begin());
        using hash_end_t = decltype(std::declval<hash_t>().hash().begin());
        hash_t hash_;
        hash_begin_t hash_begin_{};
        hash_begin_t hash_current_{};
        hash_end_t hash_end_{};
        std::array<uint8_t, H::chunk_size> chunk_{};
        typename std::array<uint8_t, H::chunk_size>::iterator chunk_current_{ chunk_.begin() };
        struct empty {};
        template <bool SingleByte, typename ValueBufT>
        struct value_buf_current_type_helper;

        template <typename ValueBufT>
        struct value_buf_current_type_helper<true, ValueBufT> {
            using type = empty; 
        };

        template <typename ValueBufT>
        struct value_buf_current_type_helper<false, ValueBufT> {
            using type = decltype(std::declval<ValueBufT>().begin());
        };
        using value_t = std::conditional_t<single_byte, empty, O>;
        using value_buf_t = std::conditional_t<single_byte, empty, std::span<uint8_t, sizeof(value_t)>>;
        using value_buf_current_t = typename value_buf_current_type_helper<single_byte, value_buf_t>::type;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-attributes"
#endif
        [[no_unique_address]] value_t value_;
        [[no_unique_address]] value_buf_t value_buf_;
        [[no_unique_address]] value_buf_current_t value_buf_current_;  // NOLINT(clang-diagnostic-padded)
#ifdef __clang__
#pragma clang diagnostic pop
#endif
        bool input_complete_{ false };
    public:
        template<bool SingleByte = single_byte>
            requires (SingleByte == true)
        explicit hash_processor(size_t hash_size)
            : hash_{ hash_size }
            , value_{}
            , value_buf_{}
            , value_buf_current_{} {
        }
        template<bool SingleByte = single_byte>
            requires (SingleByte == false)
        explicit hash_processor(size_t hash_size)
            : hash_{ hash_size }
            , value_{}
            , value_buf_{ reinterpret_cast<uint8_t*>(&value_), sizeof(O) }
            , value_buf_current_{ value_buf_.begin() } {
        }

        auto target_hash_size() const -> size_t
        {
            return hash_.target_hash_size();
        }

        auto hash_size() const -> size_t
        {
            return static_cast<size_t>(std::distance<decltype(hash_begin_)>(hash_begin_, hash_current_));
        }

        auto hash() const
        {
            return std::ranges::subrange(hash_begin_, hash_current_);
        }

        auto complete() const -> bool
        {
            return input_complete_ && hash_size() == target_hash_size();
        }

        auto input_complete() const -> bool
        {
            return input_complete_;
        }

        auto hash_position() const -> size_t
        {
            return std::distance<decltype(hash_begin_)>(hash_begin_, hash_current_);
        }

        template<typename T, next_byte_function N>
            requires std::is_standard_layout_v<T>&& single_byte && return_inputs
        auto process(N next_byte) -> T
        {
            if (input_complete_)
            {
                return hash_current_ == hash_end_ ? 0 : *hash_current_++;
            }

            if (auto [byte_ok, byte_value] {hash_next_byte(next_byte)}; byte_ok)
            {
                return static_cast<O>(byte_value);
            }

            hash_.final({ chunk_.data(), static_cast<size_t>(std::distance(chunk_.begin(), chunk_current_)) });
            auto hash{ hash_.hash() };
            hash_begin_ = hash_current_ = hash.begin();
            hash_end_ = hash.end();
            input_complete_ = true;
            return static_cast<O>(*hash_current_++);
        }

        template<typename T, next_byte_function N>
            requires (std::is_standard_layout_v<T> && single_byte && !return_inputs)
        auto process(N next_byte) -> T
        {
            if (input_complete_)
            {
                return hash_size() == target_hash_size() ? 0 : *hash_current_++;
            }

            while (true)
            {
                if (auto [byte_ok, byte_value] {hash_next_byte(next_byte)}; !byte_ok)
                {
                    break;
                }
            }

            hash_.final({ chunk_.data(), static_cast<size_t>(std::distance(chunk_.begin(), chunk_current_)) });
            auto hash{ hash_.hash() };
            hash_begin_ = hash_current_ = hash.begin();
            hash_end_ = hash.end();
            input_complete_ = true;
            return static_cast<O>(*hash_current_++);
        }

        template<typename T, next_byte_function N>
            requires (std::is_standard_layout_v<T> && !single_byte)
        auto process(N next_byte) -> T
        {
            if (input_complete_)
            {
                if (hash_size() >= target_hash_size())
                {
                    return O{};
                }

                if (static_cast<size_t>(std::distance(hash_current_, hash_end_)) < sizeof(O))
                {
                    auto hash_size{ static_cast<size_t>(std::distance(hash_begin_, hash_end_)) };
                    throw std::runtime_error(
                        std::format(
                            "Cannot handle output type size of {} bytes. {} hash bytes remaining. Not enough hash data to fill the output value. Expected {} bytes, only {}{} hash bytes available.",
                            sizeof(O),
                            std::distance(hash_current_, hash_end_),
                            hash_.target_hash_size(),
                            hash_.target_hash_size() < hash_size ? std::format(" of {}", hash_size) : std::format(""),
                            hash_size));
                }

                while (true)
                {
                    *value_buf_current_++ = *hash_current_++;
                    if (value_buf_current_ == value_buf_.end())
                    {
                        value_buf_current_ = value_buf_.begin();
                        return value_;
                    }
                }
            }

            while (true)
            {
                if (auto [byte_ok, byte_value] {hash_next_byte(next_byte)}; byte_ok)
                {
                    if constexpr (return_inputs)
                    {
                        *value_buf_current_++ = byte_value;
                        if (value_buf_current_ == value_buf_.end())
                        {
                            value_buf_current_ = value_buf_.begin();
                            return value_;
                        }
                    }
                }
                else
                {
                    break;
                }
            }

            hash_.final({ chunk_.data(), static_cast<size_t>(std::distance(chunk_.begin(), chunk_current_)) });
            if constexpr (pad_hash)
            {
                // extend the hash pad to fill up to the next multiple of sizeof(O)
                auto current_partial_byte_count{ static_cast<size_t>(std::distance(value_buf_.begin(), value_buf_current_)) };
                hash_.set_target_hash_size((((current_partial_byte_count + hash_.target_hash_size() + sizeof(O) - 1) / sizeof(O)) * sizeof(O)) - current_partial_byte_count);
            }
            auto hash{ hash_.hash() };
            hash_begin_ = hash_current_ = hash.begin();
            hash_end_ = hash.end();
            input_complete_ = true;

            while (true)
            {
                *value_buf_current_++ = *hash_current_++;
                if (value_buf_current_ == value_buf_.end())
                {
                    value_buf_current_ = value_buf_.begin();
                    return value_;
                }
            }
        }
        private:
            template<next_byte_function N>
            auto hash_next_byte(N next_byte) -> std::tuple<bool, uint8_t>
            {
                auto [byte_ok, byte_value] {next_byte()};
                if (byte_ok)
                {
                    *chunk_current_ = byte_value;
                    ++chunk_current_;
                    if (chunk_current_ == chunk_.end())
                    {
                        hash_.update(chunk_);
                        chunk_current_ = chunk_.begin();
                    }
                }

                return { byte_ok, byte_value };
            }
    };
}
