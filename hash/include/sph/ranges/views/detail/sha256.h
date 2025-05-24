#pragma once
#include <array>
#include <sodium/crypto_hash_sha256.h>
#include <sph/hash_algorithm.h>
#include <sph/hash_param.h>

namespace sph::ranges::views::detail
{

    class sha256
    {
    public:
        static constexpr size_t hash_size{ sph::hash_param<sph::hash_algorithm::sha256>::hash_size() };
        static constexpr size_t chunk_size{ sph::hash_param<sph::hash_algorithm::sha256>::chunk_size() };
    private:
        size_t target_hash_size_;
        crypto_hash_sha256_state state_;
        std::array<uint8_t, hash_size> hash_{};
    public:
        sha256(size_t target_hash_size)
            : target_hash_size_{ target_hash_size }
            , state_{ init_state() }
        {
        }

        auto target_hash_size() const -> size_t
        {
            return target_hash_size_;
        }

        auto hash() const-> std::span<uint8_t const, sph::hash_param<sph::hash_algorithm::sha256>::hash_size()>
        {
            return hash_;
        }

        auto update(std::span<uint8_t const, chunk_size> const data) -> void
        {
            crypto_hash_sha256_update(&state_, data.data(), data.size());
        }

        auto final(std::span<uint8_t const> const data) -> void
        {
            if (!data.empty())
            {
                crypto_hash_sha256_update(&state_, data.data(), data.size());
            }

            crypto_hash_sha256_final(&state_, hash_.data());
        }
        
    private:
        static auto init_state() -> crypto_hash_sha256_state
        {
            crypto_hash_sha256_state state;
            crypto_hash_sha256_init(&state);
            return state;
        }
    };

}