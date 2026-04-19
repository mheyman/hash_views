#pragma once
#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <stdexcept>
#include <sodium/crypto_generichash_blake2b.h>
#include <sph/blake2b_parameters.h>
#include <sph/hash_algorithm.h>
#include <sph/hash_param.h>

namespace sph::ranges::views::detail
{
    class CRYPTO_ALIGN(64) blake2b
    {
    public:
        static constexpr size_t hash_size{ sph::hash_param<sph::hash_algorithm::blake2b>::hash_byte_count() };
        static constexpr size_t chunk_size{ sph::hash_param<sph::hash_algorithm::blake2b>::chunk_size() };
        static constexpr size_t salt_size{ crypto_generichash_blake2b_SALTBYTES };
        static constexpr size_t personal_size{ crypto_generichash_blake2b_PERSONALBYTES };
    private:
        crypto_generichash_blake2b_state state_;
        std::array<uint8_t, hash_size> hash_{};
        size_t hash_size_;
        [[maybe_unused]] std::array<uint8_t, hash_size - sizeof(size_t)> unused_pad_{};
    public:
        explicit blake2b(size_t hash_byte_count, sph::blake2b_parameters parameters = {})
            : state_{ init_state(hash_byte_count, parameters) }
            , hash_size_{ hash_byte_count }
        {
        }

        auto target_hash_size() const -> size_t
        {
            return hash_size_;
        }

        auto hash() const -> std::span<uint8_t const>
        {
            return {hash_.data(), hash_size_};
        }

        auto update(std::span<uint8_t const, chunk_size> const data) -> void
        {
            crypto_generichash_blake2b_update(&state_, data.data(), data.size());
        }

        auto final(std::span<uint8_t const> const data) -> void
        {
            if (!data.empty())
            {
                crypto_generichash_blake2b_update(&state_, data.data(), data.size());
            }

            crypto_generichash_blake2b_final(&state_, hash_.data(), hash_.size());
        }

    private:
        static auto init_state(size_t hash_byte_count, sph::blake2b_parameters parameters) -> crypto_generichash_blake2b_state
        {
            validate_parameters(parameters);
            crypto_generichash_blake2b_state state;
            if (parameters.salt.empty() && parameters.personal.empty())
            {
                crypto_generichash_blake2b_init(&state,
                    parameters.key.empty() ? nullptr : parameters.key.data(),
                    parameters.key.size(),
                    hash_byte_count);
                return state;
            }

            std::array<uint8_t, salt_size> salt{};
            std::array<uint8_t, personal_size> personal{};
            std::ranges::copy(parameters.salt, salt.begin());
            std::ranges::copy(parameters.personal, personal.begin());
            crypto_generichash_blake2b_init_salt_personal(&state,
                parameters.key.empty() ? nullptr : parameters.key.data(),
                parameters.key.size(),
                hash_byte_count,
                salt.data(),
                personal.data());
            return state;
        }

        static auto validate_parameters(sph::blake2b_parameters parameters) -> void
        {
            if (!parameters.key.empty()
                && parameters.key.size() > crypto_generichash_blake2b_KEYBYTES_MAX)
            {
                throw std::invalid_argument(
                    std::format("BLAKE2b key length must be between 0 and {} bytes, got {}.",
                        crypto_generichash_blake2b_KEYBYTES_MAX,
                        parameters.key.size()));
            }

            if (!parameters.salt.empty() && parameters.salt.size() != salt_size)
            {
                throw std::invalid_argument(
                    std::format("BLAKE2b salt length must be 0 or {} bytes, got {}.",
                        salt_size,
                        parameters.salt.size()));
            }

            if (!parameters.personal.empty() && parameters.personal.size() != personal_size)
            {
                throw std::invalid_argument(
                    std::format("BLAKE2b personalization length must be 0 or {} bytes, got {}.",
                        personal_size,
                        parameters.personal.size()));
            }
        }
    };
}
