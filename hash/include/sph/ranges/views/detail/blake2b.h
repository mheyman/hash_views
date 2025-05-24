#pragma once
#include <array>
#include <sodium/crypto_generichash_blake2b.h>
#include <sph/hash_algorithm.h>
#include <sph/hash_param.h>

namespace sph::ranges::views::detail
{
    class CRYPTO_ALIGN(64) blake2b
    {
    public:
        static constexpr size_t hash_size{ sph::hash_param<sph::hash_algorithm::blake2b>::hash_size() };
        static constexpr size_t chunk_size{ sph::hash_param<sph::hash_algorithm::blake2b>::chunk_size() };
    private:
        crypto_generichash_blake2b_state state_;
        std::array<uint8_t, hash_size> hash_{};
        size_t target_hash_size_;
        [[maybe_unused]] std::array<uint8_t, hash_size - sizeof(size_t)> unused_pad_{};
    public:
        blake2b(size_t target_hash_size)
            : state_{ init_state(target_hash_size) }
            , target_hash_size_{ target_hash_size }
        {
        }

        auto target_hash_size() const -> size_t
        {
            return target_hash_size_;
        }

        auto hash() const -> std::span<uint8_t const, hash_size>
        {
            return hash_;
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
        static auto init_state(size_t target_hash_size) -> crypto_generichash_blake2b_state
        {
            crypto_generichash_blake2b_state state;
            crypto_generichash_blake2b_init(&state, nullptr, 0, target_hash_size);
            return state;
        }
    };
}