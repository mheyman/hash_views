#pragma once
#include <sph/hash_algorithm.h>
namespace sph
{
    template<hash_algorithm A>
    struct hash_param
    {
        static constexpr auto hash_byte_count() -> size_t
        {
            if constexpr (A == hash_algorithm::sha256)
            {
                return 32;
            }
            else if constexpr (A == hash_algorithm::sha512)
            {
                return 64;
            }
            else if constexpr (A == hash_algorithm::blake2b)
            {
                return 64;
            }
            else
            {
                throw std::invalid_argument("Unsupported hash algorithm");
            }
        }

        static constexpr auto chunk_size() -> size_t
        {
            if constexpr (A == hash_algorithm::sha256)
            {
                return 64;
            }
            else if constexpr (A == hash_algorithm::sha512)
            {
                return 128;
            }
            else if constexpr (A == hash_algorithm::blake2b)
            {
                return 128;
            }
            else
            {
                throw std::invalid_argument("Unsupported hash algorithm");
            }
        }

        static constexpr auto name() -> std::string_view
        {
            if constexpr (A == hash_algorithm::sha256)
            {
                return "SHA256";
            }
            else if constexpr (A == hash_algorithm::sha512)
            {
                return "SHA512";
            }
            else if constexpr (A == hash_algorithm::blake2b)
            {
                return "BLAKE2B";
            }
            else
            {
                throw std::invalid_argument("Unsupported hash algorithm");
            }
        }
    };
}