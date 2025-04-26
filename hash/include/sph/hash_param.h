#pragma once
#include <sph/hash_algorithm.h>
namespace sph
{
    template<hash_algorithm A>
    class hash_param
    {
        static constexpr auto get_hash_size() -> size_t
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

        static constexpr auto get_hash_chunk_size() -> size_t
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
                return 256;
            }
            else
            {
                throw std::invalid_argument("Unsupported hash algorithm");
            }
        }

        static constexpr auto get_type_name() -> std::string_view
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
    public:
        /**
         * Maximum number of uint8_t elements in a hash.
         */
        static constexpr auto hash_size { hash_param::get_hash_size()};

        /**
         * How many bytes in a chunk of data to hash at a time.
         */
        static constexpr auto hash_chunk_size{ hash_param::get_hash_chunk_size() };

        /**
         * Text name of the hash algorithm.
         */
        static constexpr auto type_name { hash_param::get_type_name()};
    };
}