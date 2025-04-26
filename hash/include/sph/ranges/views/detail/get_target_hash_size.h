#pragma once
#include <sph/hash_param.h>
namespace sph::ranges::views::detail
{
    /**
     * Gets the target hash size from a target hash size proposal.
     * 
     */
    template<typename T, sph::hash_algorithm A>
        requires std::is_standard_layout_v<T> 
    auto static constexpr get_target_hash_size(size_t proposed_target_hash_size) -> size_t
    {
        if (proposed_target_hash_size > sph::hash_param<A>::hash_size)
        {
            throw std::invalid_argument(std::format("Hash size {} is larger than maximum hash size {}", proposed_target_hash_size, sph::hash_param<A>::hash_size));
        }

        if constexpr (sizeof(T) == 1)
        {
            if (proposed_target_hash_size == 0)
            {
                return sph::hash_param<A>::hash_size;
            }

            return proposed_target_hash_size;
        }
        else
        {
            if (proposed_target_hash_size == 0)
            {
                if (sph::hash_param<A>::hash_size > sizeof(T))
                {
                    // fill up the last output value with the hash
                    return sph::hash_param<A>::hash_size - sizeof(T) + 1;
                }

                // The last value could be too big and we won't know until we get there.
                return 1;
            }

            // The last value could be too big and we won't know until we get there.
            return proposed_target_hash_size;
        }
    }

}