#pragma once
#include <cassert>
#include <tuple>
#include <type_traits>
#include <sph/hash_algorithm.h>
#include <sph/hash_format.h>
#include <sph/hash_site.h>
#include <sph/ranges/views/detail/blake2b.h>
#include <sph/ranges/views/detail/get_hash_size.h>
#include <sph/ranges/views/detail/hash_processor.h>
#include <sph/ranges/views/detail/sha256.h>
#include <sph/ranges/views/detail/sha512.h>
#include <sph/ranges/views/detail/rolling_buffer.h>
#include <sodium/crypto_generichash_blake2b.h>

namespace sph::ranges::views::detail
{
    enum class end_of_input : uint8_t
    {
        no_appended_hash = 0,
        skip_appended_hash = 1
    };

    struct hash_iterator_empty {};

    // Primary template: fallback
    template <hashable_type T, sph::hash_algorithm A, bool UseRolling>
    struct select_rolling_buffer_type;

    template <hashable_type T, sph::hash_algorithm A>
    struct select_rolling_buffer_type<T, A, true> {
        using type = rolling_buffer<T, A>;
    };

    template <hashable_type T, sph::hash_algorithm A>
    struct select_rolling_buffer_type<T, A, false> {
        using type = hash_iterator_empty;
    };


    /**
     * Forward declaration of the iterator end-of-sequence
     * sentinel.
     */
    template<hash_range R, hashable_type T, sph::hash_algorithm A, sph::hash_format F, sph::hash_site S, end_of_input E>
    struct hash_sentinel;

    /**
     * The iterator used by the hash_view and hash_verify_view
     * providing a view of the hashed stream.
     *
     * @tparam R The type of the range that holds a hashed stream.
     * @tparam T The output type.
     * @tparam A The hash algorithm to use.
     * @tparam S The hash style to use (append to hashed data or separate from hashed data). If sizeof(T) == 1, padded hashes are not acceptable.
     * @tparam E The iterate style to use (skip appended hash or no appended hash).
     */
    template<hash_range R, hashable_type T, sph::hash_algorithm A, sph::hash_format F, sph::hash_site S, end_of_input E>
    class hash_iterator  // NOLINT(clang-diagnostic-padded)
    {
        using const_hashed_iterator_t = std::ranges::const_iterator_t<std::remove_reference_t<R>>;
        using const_hashed_sentinel_t = std::ranges::const_sentinel_t<std::remove_reference_t<R>>;
    public:
        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::remove_cvref_t<T>;
        using pointer = const value_type*;
        using reference = const value_type&;
        using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
        using output_type = std::remove_cvref_t<T>;
    private:
        static constexpr bool single_byte_input{ sizeof(input_type) == 1 };
        struct input_value_with_position { input_type value; size_t position; };
        using input_value_t = std::conditional_t < sizeof(input_type) == 1, hash_iterator_empty, input_value_with_position>;
        using hash_processor_t = hash_processor<T, S, F, std::conditional_t<A == sph::hash_algorithm::blake2b, detail::blake2b, 
            std::conditional_t<A == sph::hash_algorithm::sha512, detail::sha512,
            std::conditional_t<A == sph::hash_algorithm::sha256, detail::sha256, void>>>>;
        using rolling_buffer_t = typename select_rolling_buffer_type<T, A, E == end_of_input::skip_appended_hash>::type;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-attributes"
#endif
        [[no_unique_address]] input_value_t input_{input_init()};
        [[no_unique_address]] rolling_buffer_t rolling_buffer_;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
        std::unique_ptr<hash_processor_t> hash_;
        const_hashed_iterator_t to_hash_current_;
        const_hashed_sentinel_t to_hash_end_;
        T value_;
        mutable bool hash_read_complete_{ false };
        mutable bool complete_{ false };
    public:
        /**
         * Initialize a new instance of the hash_iterator class.
         * @param begin The start of the input range to hash.
         * @param end The end of the input range.
         * @param hash_byte_count The size of the hash to create. Zero gives
         * largest size available. If the size of the output is greater than 1
         * and the hash style doesn't include padding, the hash may not be
         * storable and an exception will be thrown when that point in the
         * output range is reached.
         */
        hash_iterator(const_hashed_iterator_t begin, const_hashed_sentinel_t end, size_t hash_byte_count)
            : hash_{ std::make_unique<hash_processor_t>(get_hash_size<A>(hash_byte_count)) }
            , to_hash_current_(std::move(begin))
            , to_hash_end_(std::move(end))
            , value_{ hash_->template process<T>([this]() -> std::tuple<bool, uint8_t> { return next_byte(); }) }
        {
        }

        hash_iterator(hash_iterator<R, T, A, F, S, E>& o) noexcept
            : hash_{ nullptr } // only one can hash at a time
            , to_hash_current_{o.to_hash_current_}
            , to_hash_end_{ o.to_hash_end_ }
            , value_{ o.value_ }
        {
        }
        hash_iterator(hash_iterator<R, T, A, F, S, E>&&) noexcept = default;
        ~hash_iterator() = default;
        auto operator=(hash_iterator<R, T, A, F, S, E> const& o) noexcept -> hash_iterator&
        {
            if (&o != this)
            {
                hash_.reset(); // only one can hash at a time
                to_hash_current_ = o.to_hash_current_;
                to_hash_end_ = o.to_hash_end_;
                value_ = o.value_;
            }

            return *this;
            
        }
        auto operator=(hash_iterator&&) noexcept -> hash_iterator& = default;

        auto hash_size() const -> size_t
        {
            verify_can_hash();
            return hash_->hash_size();
        }

        auto hash() const -> std::ranges::subrange<decltype(hash_->hash().begin()), decltype(hash_->hash().end())>
        {
            verify_can_hash();
            return hash_->hash();
        }

        /**
         * Increment the iterator.
         * @return The pre-incremented iterator value.
         */
        auto operator++(int) -> hash_iterator
        {
            hash_iterator ret{ *this };
            if (!hash_read_complete_)
            {
                verify_can_hash();
                value_ = hash_->template process<T>([this]() -> std::tuple<bool, uint8_t> { return next_byte(); });
                hash_read_complete_ = hash_->complete();
            }
            else
            {
                complete_ = true;
            }

            return ret;
        }

        /**
         * Increment the iterator.
         * @return The incremented iterator value.
         */
        auto operator++() -> hash_iterator&
        {
            if (!hash_read_complete_)
            {
                verify_can_hash();
                value_ = hash_->template process<T>([this]() -> std::tuple<bool, uint8_t> { return next_byte(); });
                hash_read_complete_ = hash_->complete();
            }
            else
            {
                complete_ = true;
            }

            return *this;
        }

        /**
         * Gets the current hashed value.
         * @return The current hashed value.
         */
        auto operator*() const -> output_type
        {
            assert(!complete_ && "Cannot dereference end of hash iterator.");
            return value_;
        }

        /**
         * Compare the provided iterator for equality.
         * @param i The iterator to compare against.
         * @return True if the provided iterator is the same as this one.
         */
        auto equals(const hash_iterator& i) const noexcept -> bool
        requires single_byte_input
        {
            return to_hash_current_ == i.to_hash_current_ 
                && to_hash_end_ == i.to_hash_end_
                && (
                    (!hash_ && !i.hash_) 
                    || 
                    (
                        hash_ && i.hash_ 
                        && hash_->input_complete() == i.hash_->input_complete()
                        && hash_->hash_position() == i.hash_->hash_position()));
        }

        /**
         * Compare the provided iterator for equality.
         * @param i The iterator to compare against.
         * @return True if the provided iterator is the same as this one.
         */
        auto equals(const hash_iterator& i) const noexcept -> bool
        requires (!single_byte_input)
        {
            return to_hash_current_ == i.to_hash_current_
                && to_hash_end_ == i.to_hash_end_
                && input_.position == i.input_.position
                && (
                    (!hash_ && !i.hash_)
                    ||
                    (hash_ && i.hash_
                        && hash_->input_complete() == i.hash_->input_complete()
                        && hash_->hash_position() == i.hash_->hash_position()));
        }

        /**
         * Compare the provided sentinel for equality.
         * @return True if at the end of the hashed view.
         */
        auto equals(const hash_sentinel<R, T, A, F, S, E>&) const noexcept -> bool
        {
            return complete_;
        }

        auto operator==(const hash_iterator& other) const noexcept -> bool { return equals(other); }
        auto operator==(const hash_sentinel<R, T, A, F, S, E>& s) const noexcept -> bool { return equals(s); }
        auto operator!=(const hash_iterator& other) const noexcept -> bool { return !equals(other); }
        auto operator!=(const hash_sentinel<R, T, A, F, S, E>& s) const noexcept -> bool { return !equals(s); }

    private:
        auto verify_can_hash() const -> void
        {
            if (!hash_)
            {
                throw std::runtime_error("Only one copy of the hash iterator can hash. You probably made a copy of the iterator and tried to use it. Moving the iterator is fine.");
            }
        }

        auto verify_can_increment() const -> void
        {
            verify_can_hash();
            if (hash_read_complete_)
            {
                throw std::runtime_error("Attempt to increment past end of hash.");
            }
        }

        static auto input_init()
        requires single_byte_input
        {
            return hash_iterator_empty{};
        }

        auto input_complete() -> bool
            requires single_byte_input
        {
            return to_hash_current_ == to_hash_end_;
        }


        auto next_byte_from_input_range() -> std::tuple<bool, uint8_t>
        requires single_byte_input
        {
            if (input_complete())
            {
                // not sure can get here if it isn't an error - maybe should be a throw
                return { false, static_cast<uint8_t>(0) };
            }

            return { true, *to_hash_current_++ };
        }

        static auto input_init()
        requires (!single_byte_input)
        {
            return input_value_with_position{ {}, sizeof(input_type) };
        }



        auto input_complete() -> bool
        requires (!single_byte_input)
        {
            return input_.position == sizeof(input_type) && to_hash_current_ == to_hash_end_;
        }

        auto next_byte_from_input_range() -> std::tuple<bool, uint8_t>
        requires (!single_byte_input)
        {
            if (input_complete())
            {
                // not sure can get here if it isn't an error - maybe should be a throw
                return { false, static_cast<uint8_t>(0) };
            }

            if (input_.position == sizeof(input_type))
            {
                input_.position = 0;
                input_.value = *to_hash_current_;
                ++to_hash_current_;
            }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-container"
#endif
            std::tuple<bool, uint8_t> ret{ true, (std::span<uint8_t, sizeof(input_type)>{ reinterpret_cast<uint8_t*>(&input_.value), sizeof(input_type) })[input_.position] };
#ifdef __clang__
#pragma clang diagnostic pop
#endif
            ++input_.position;
            return ret;
        }

        auto next_byte() -> std::tuple<bool, uint8_t>
        requires (E == end_of_input::skip_appended_hash)
        {
            if (rolling_buffer_.done())
            {
                return rolling_buffer_.next();
            }

            while(true)
            {
                if (auto [valid_value, value] {next_byte_from_input_range()}; valid_value)
                {
                    auto b{ rolling_buffer_.next(value) };
                    if (b)
                    {
                        return { true, *b };
                    }
                }
                else
                {
                    rolling_buffer_.done(hash_->target_hash_size());
                    return rolling_buffer_.next();
                }
            }
        }

        auto next_byte() -> std::tuple<bool, uint8_t>
        requires (E != end_of_input::skip_appended_hash)
        {
            return next_byte_from_input_range();
        }
    };

    template<hash_range R, hashable_type T, sph::hash_algorithm A, sph::hash_format F, sph::hash_site S, end_of_input E>
    struct hash_sentinel
    {
        auto operator==(const hash_sentinel& /*other*/) const noexcept -> bool { return true; }
        auto operator==(const hash_iterator<R, T, A, F, S, E>& i) const noexcept -> bool { return i.equals(*this); }
        auto operator!=(const hash_sentinel& /*other*/) const noexcept -> bool { return false; }
        auto operator!=(const hash_iterator<R, T, A, F, S, E>& i) const noexcept -> bool { return !i.equals(*this); }
    };
}
