#pragma once
#include <array>
#include <sodium/crypto_hash_sha512.h>
#include <sph/hash_algorithm.h>
#include <sph/hash_param.h>

namespace sph::ranges::views::detail
{
    class sha512
    {
    public:
        static constexpr size_t hash_size{ sph::hash_param<sph::hash_algorithm::sha512>::hash_byte_count() };
        static constexpr size_t chunk_size{ sph::hash_param<sph::hash_algorithm::sha512>::chunk_size() };
    private:
        size_t hash_size_{};
        crypto_hash_sha512_state state_;
        std::array<uint8_t, hash_size> hash_{};
    public:
        sha512(size_t hash_byte_count)
            : hash_size_{ hash_byte_count }
            , state_{ init_state() }
        {
        }

        auto target_hash_size() const -> size_t
        {
            return hash_size_;
        }

        auto hash() const -> std::span<uint8_t const>
        {
            return { hash_.data(), hash_size_ };
        }

        auto update(std::span<uint8_t const, chunk_size> const data) -> void
        {
            crypto_hash_sha512_update(&state_, data.data(), data.size());
        }

        auto final(std::span<uint8_t const> const data) -> void
        {
            if (!data.empty())
            {
                crypto_hash_sha512_update(&state_, data.data(), data.size());
            }

            crypto_hash_sha512_final(&state_, hash_.data());
        }

    private:
        static auto init_state() -> crypto_hash_sha512_state
        {
            crypto_hash_sha512_state state;
            crypto_hash_sha512_init(&state);
            return state;
        }
    };
}