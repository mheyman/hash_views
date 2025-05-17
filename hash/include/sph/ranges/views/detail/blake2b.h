#pragma once
#include <array>
#include <sodium/crypto_generichash_blake2b.h>
#include <sph/hash_algorithm.h>
#include <sph/hash_param.h>
#include <sph/hash_style.h>
#include <sph/ranges/views/detail/get_target_hash_size.h>
#include <sph/ranges/views/detail/process_util.h>
#include <type_traits>
namespace sph::ranges::views::detail
{
    template<typename O, sph::hash_style S>
    class blake2b
    {
        static bool constexpr return_inputs{ S == sph::hash_style::append };
        static bool constexpr single_byte{ sizeof(O) == 1 };
        size_t target_hash_size_;
        crypto_generichash_blake2b_state state_;
        std::array<uint8_t, sph::hash_param<sph::hash_algorithm::blake2b>::hash_size()> hash_{};
        std::array<uint8_t, sph::hash_param<sph::hash_algorithm::blake2b>::hash_size()>::iterator hash_current_{ hash_.begin() };
        std::array<uint8_t, sph::hash_param<sph::hash_algorithm::blake2b>::chunk_size()> chunk_{};
        std::array<uint8_t, sph::hash_param<sph::hash_algorithm::blake2b>::chunk_size()>::iterator chunk_current_{ chunk_.begin() };
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
        [[no_unique_address]] value_buf_current_t value_buf_current_;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
        bool input_complete_{ false };
    public:
        template<bool SINGLE_BYTE = single_byte>
            requires (SINGLE_BYTE == true)
        explicit blake2b(size_t target_hash_size) 
            : target_hash_size_{get_target_hash_size<O, sph::hash_algorithm::blake2b>(target_hash_size)}
            , state_{init_state(target_hash_size_)}
            , value_{}
            , value_buf_{}
            , value_buf_current_{} {}
        template<bool SINGLE_BYTE = single_byte>
            requires (SINGLE_BYTE == false)
        explicit blake2b(size_t target_hash_size)
            : target_hash_size_{ get_target_hash_size<O, sph::hash_algorithm::blake2b>(target_hash_size) }
            , state_{ init_state(target_hash_size_) }
            , value_{}
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-container"
#endif
            , value_buf_{ value_buf_t(reinterpret_cast<uint8_t*>(&value_), sizeof(O)) }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
            , value_buf_current_{ value_buf_.begin() } {}

        auto target_hash_size() const -> size_t
        {
            return target_hash_size_;
        }

        auto hash_size() const -> size_t
        {
            return std::distance<decltype(hash_.begin())>(hash_.begin(), hash_current_);
        }

        auto complete() -> bool
        {
            return input_complete_ && std::distance(hash_.begin(), hash_current_) >= static_cast<ptrdiff_t>(target_hash_size_);
        }

        auto input_complete() const -> bool
        {
            return input_complete_;
        }

        auto hash_position() const -> size_t
        {
            return std::distance<decltype(hash_.begin())>(hash_.begin(), hash_current_);
        }

        template<typename T, next_byte_function F>
            requires std::is_standard_layout_v<T>
        auto process(F next_byte) -> T
        {
            if constexpr (single_byte)
            {
                if (input_complete_)
                {
                    return std::distance(hash_.begin(), hash_current_) == static_cast<ptrdiff_t>(target_hash_size_) ? 0 : *hash_current_++;
                }

                while (true)
                {
                    if (auto [byte_ok, byte_value] {next_byte()}; byte_ok)
                    {
                        *chunk_current_ = byte_value;
                        if (chunk_current_ == chunk_.end())
                        {
                            crypto_generichash_blake2b_update(&state_, chunk_.data(), chunk_.size());
                            chunk_current_ = chunk_.begin();
                        }

                        if constexpr (return_inputs)
                        {
                            return static_cast<O>(byte_value);
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                if (chunk_current_ != chunk_.begin())
                {
                    crypto_generichash_blake2b_update(&state_, chunk_.data(), static_cast<size_t>(std::distance(chunk_.begin(), chunk_current_)));
                }

                crypto_generichash_blake2b_final(&state_, hash_.data(), hash_.size());
                input_complete_ = true;
                return static_cast<O>(*hash_current_++);
            }
            else
            {
                if (input_complete_)
                {
                    if (std::distance(hash_.begin(), hash_current_) >= target_hash_size_)
                    {
                        return O{};
                    }

                    if (std::distance(hash_current_, hash_.end()) < sizeof(O))
                    {
                        throw std::runtime_error(
                            std::format(
                                "Cannot handle output type size of {} bytes. Not enough hash data to fill the output value. Expected {} bytes, only {}{} hash bytes available.",
                                sizeof(O),
                                target_hash_size_,
                                target_hash_size_ < hash_.size() ? std::format(" of {}", hash_.size()) : std::format(""),
                                hash_.size()));
                    }

                    while(true)
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
                    if (auto [byte_ok, byte_value] {next_byte()}; byte_ok)
                    {
                        *chunk_current_ = byte_value;
                        if (chunk_current_ == chunk_.end())
                        {
                            crypto_generichash_blake2b_update(&state_, chunk_.data(), chunk_.size());
                            chunk_current_ = chunk_.begin();
                        }

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

                if (chunk_current_ != chunk_.begin())
                {
                    crypto_generichash_blake2b_update(&state_, chunk_.data(), std::distance(chunk_.begin(), chunk_current_));
                }

                crypto_generichash_blake2b_final(&state_, hash_.data(), hash_.size());
                input_complete_ = true;

                while(true)
                {
                    *value_buf_current_++ = *hash_current_++;
                    if (value_buf_current_ == value_buf_.end())
                    {
                        value_buf_current_ = value_buf_.begin();
                        return value_;
                    }
                }
            }
        }
        private:
            static auto init_state(size_t target_hash_size) -> crypto_generichash_blake2b_state
            {
                crypto_generichash_blake2b_state state;
                crypto_generichash_blake2b_init(&state, nullptr, 0, target_hash_size);
                return state;
            }
    };
}