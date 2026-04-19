#pragma once
#include <array>
#include <span>
#include <blake3.h>
#include <sph/hash_algorithm.h>
#include <sph/hash_param.h>

namespace sph::ranges::views::detail
{
    class blake3
    {
    public:
        static constexpr size_t hash_size{ sph::hash_param<sph::hash_algorithm::blake3>::hash_byte_count() };
        static constexpr size_t chunk_size{ sph::hash_param<sph::hash_algorithm::blake3>::chunk_size() };
    private:
        size_t hash_size_{};
        blake3_hasher state_;
        std::array<uint8_t, hash_size> hash_{};
    public:
        explicit blake3(size_t hash_byte_count)
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
            update_impl(data);
        }

        auto final(std::span<uint8_t const> const data) -> void
        {
            if (!data.empty())
            {
                update_impl(data);
            }

            blake3_hasher_finalize(&state_, hash_.data(), hash_size_);
        }

    private:
        static auto init_state() -> blake3_hasher
        {
            blake3_hasher state;
            blake3_hasher_init(&state);
            return state;
        }

        auto update_impl(std::span<uint8_t const> data) -> void
        {
#if defined(BLAKE3_USE_TBB)
            blake3_hasher_update_tbb(&state_, data.data(), data.size());
#else
            blake3_hasher_update(&state_, data.data(), data.size());
#endif
        }
    };
}
