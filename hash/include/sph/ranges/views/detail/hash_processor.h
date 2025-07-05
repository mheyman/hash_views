#pragma once
#include <type_traits>
#include <sph/hash_style.h>
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
     * @tparam S The hash style. Append, append padded, separate, or separate padded.
     * @tparam H The hash. Sha256, sha512, black2b.
     */
    template<typename O, sph::hash_style S, basic_hash H>
    class hash_processor  // NOLINT(clang-diagnostic-padded)
    {
        static constexpr bool return_inputs{ S == sph::hash_style::append || S == sph::hash_style::append_padded };
        static constexpr bool single_byte{ sizeof(O) == 1 };
        static constexpr bool pad_hash{ S == sph::hash_style::append_padded || S == sph::hash_style::separate_padded };
        using hash_t = std::conditional_t<pad_hash, padded_hash<O, H>, H>;
        using hash_begin_t = decltype(std::declval<hash_t>().hash().begin());
        using hash_end_t = decltype(std::declval<hash_t>().hash().begin());
        hash_t hash_;
        hash_begin_t hash_current_{};
        hash_end_t hash_end_{};
        std::array<uint8_t, H::chunk_size> chunk_{};
        typename std::array<uint8_t, H::chunk_size>::iterator chunk_current_{ chunk_.begin() };
        struct empty {};
        using value_t = std::conditional_t<single_byte, empty, O>;
        using value_buf_t = std::conditional_t<single_byte, empty, std::array<uint8_t, sizeof(O)>>;
        using value_buf_current_t = std::conditional_t<single_byte, empty, typename std::array<uint8_t, sizeof(O)>::iterator>;
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
        template<bool SINGLE_BYTE = single_byte>
            requires (SINGLE_BYTE == true)
        explicit hash_processor(size_t hash_size)
            : hash_{ hash_size }
            , value_{}
            , value_buf_{}
            , value_buf_current_{} {
        }
        template<bool SINGLE_BYTE = single_byte>
            requires (SINGLE_BYTE == false)
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
            return static_cast<size_t>(std::distance<decltype(hash_.hash().begin())>(hash_.hash().begin(), hash_current_));
        }

        auto hash() const
        {
            return std::ranges::subrange(hash_.hash().begin(), hash_current_);
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
            return std::distance<decltype(hash_.hash().begin())>(hash_.hash().begin(), hash_current_);
        }

        template<typename T, next_byte_function F>
            requires std::is_standard_layout_v<T>&& single_byte && return_inputs
        auto process(F next_byte) -> T
        {
            if (input_complete_)
            {
                return hash_current_ == hash_end_ ? 0 : *hash_current_++;
            }

            while (true)
            {
                if (auto [byte_ok, byte_value] {hash_next_byte(next_byte)}; byte_ok)
                {
                    return static_cast<O>(byte_value);
                }
                else
                {
                    break;
                }
            }

            hash_.final({ chunk_.data(), static_cast<size_t>(std::distance(chunk_.begin(), chunk_current_)) });
            auto hash{ hash_.hash() };
            hash_current_ = hash.begin();
            hash_end_ = hash.end();
            input_complete_ = true;
            return static_cast<O>(*hash_current_++);
        }

        template<typename T, next_byte_function F>
            requires std::is_standard_layout_v<T>&& single_byte && !return_inputs
        auto process(F next_byte) -> T
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
            hash_current_ = hash.begin();
            hash_end_ = hash.end();
            input_complete_ = true;
            return static_cast<O>(*hash_current_++);
        }


        template<typename T, next_byte_function F>
            requires std::is_standard_layout_v<T> && !single_byte
        auto process(F next_byte) -> T
        {
            if (input_complete_)
            {
                if (hash_size() >= target_hash_size())
                {
                    return O{};
                }

                if (std::distance(hash_current_, hash_.hash().end()) < sizeof(O))
                {
                    throw std::runtime_error(
                        std::format(
                            "Cannot handle output type size of {} bytes. Not enough hash data to fill the output value. Expected {} bytes, only {}{} hash bytes available.",
                            sizeof(O),
                            hash_.target_hash_size(),
                            hash_.target_hash_size() < hash_.hash().size() ? std::format(" of {}", hash_.hash().size()) : std::format(""),
                            hash_.hash().size()));
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

            hash_.final({ chunk_.data(), std::distance(chunk_.begin(), chunk_current_) });
            if constexpr (pad_hash)
            {
                size_t remaining{ static_cast<size_t>(std::distance(value_buf_current_, value_buf_.end())) + hash_.target_hash_size()};
                size_t pad_size{ remaining % sizeof(O) };
            }
            hash_.set_target_hash_size(hash_.target_hash_size() + std::distance(value_buf_current_, value_buf_.end()) - 1);
            auto hash{ hash_.hash() };
            hash_current_ = hash.begin();
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
            template<next_byte_function F>
            auto hash_next_byte(F next_byte) -> std::tuple<bool, uint8_t>
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
