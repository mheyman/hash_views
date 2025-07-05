#pragma once
#include <sph/hash_param.h>
namespace sph::ranges::views::detail
{
    /**
     * Gets the hash size from the given hash size. Verifies the requested hash
     * size is not too large and converts a zero hash size to the algorithm's
     * maximum hash size.
     * 
     * @tparam A The hash algorithm used to create the hashed view.
     * @param hash_size The proposed target hash size; zero to use the maximum target hash size.
     * @return The target hash size.
     */
    template<sph::hash_algorithm A>
    auto static constexpr get_hash_size(size_t hash_size) -> size_t
    {
        if (hash_size > sph::hash_param<A>::hash_size())
        {
            throw std::invalid_argument(std::format("Hash size {} is larger than maximum hash size {}", hash_size, sph::hash_param<A>::hash_size()));
        }

        if (hash_size == 0)
        {
            // the actual hash size is the maximum hash size.
            return sph::hash_param<A>::hash_size();
        }

        // The last value could be too big, and we won't know until we get there.
        return hash_size;
    }

}