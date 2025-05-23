#pragma once
#include <tuple>
#include <type_traits>
#include <sph/hash_algorithm.h>
#include <sph/hash_style.h>
#include <sph/ranges/views/detail/blake2b.h>
#include <sph/ranges/views/detail/hash_processor.h>
#include <sph/ranges/views/detail/sha256.h>
#include <sph/ranges/views/detail/sha512.h>
#include <sph/ranges/views/detail/rolling_buffer.h>
#include <sodium/crypto_generichash_blake2b.h>

namespace sph
{
    enum class hash_style;
    enum class hash_algorithm;
}

namespace sph::ranges::views::detail
{
    enum class iterate_style : uint8_t
    {
        no_appended_hash = 0,
        skip_appended_hash = 1
    };


    /**
     * Forward declaration of the iterator end-of-sequence
     * sentinel.
     */
    template<std::ranges::viewable_range R, typename T, sph::hash_algorithm A, sph::hash_style S, iterate_style IS>
        requires std::ranges::input_range<R>&& std::is_standard_layout_v<T>&& std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
    struct hash_sentinel;

    /**
     * The iterator used by the hash_view and hash_verify_view
     * providing a view of the hashed stream.
     *
     * @tparam R The type of the range that holds a hashed stream.
     * @tparam T The output type.
     * @tparam A The hash algorithm to use.
     * @tparam S The hash style to use (append to hashed data or separate from hashed data). If sizeof(T) == 1, padded hashes are not acceptable.
     * @tparam IS The iterate style to use (skip appended hash or no appended hash).
     */
    template<std::ranges::viewable_range R, typename T, sph::hash_algorithm A, sph::hash_style S, iterate_style IS>
        requires std::ranges::input_range<R> && std::is_standard_layout_v<T>
        && std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
            && (sizeof(T) > 1 || S == sph::hash_style::separate || S == sph::hash_style::append)
    class hash_iterator  // NOLINT(clang-diagnostic-padded)
    {
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
        struct empty {};
        struct input_value_with_position { input_type value; size_t position; };
        using input_value_t = std::conditional_t < sizeof(input_type) == 1, empty, input_value_with_position>;
        using hash_processor_t = hash_processor<T, S, std::conditional_t<A == sph::hash_algorithm::blake2b, detail::blake2b, 
            std::conditional_t<A == sph::hash_algorithm::sha512, detail::sha512,
            std::conditional_t<A == sph::hash_algorithm::sha256, detail::sha256, void>>>>;
        using rolling_buffer_t = std::conditional<IS == iterate_style::no_appended_hash, empty, std::conditional<IS == iterate_style::skip_appended_hash, rolling_buffer<T, A>, void>>;
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
        std::ranges::const_iterator_t<R> to_hash_current_;
        std::ranges::const_sentinel_t<R> to_hash_end_;
        T value_;
    public:
        /**
         * Initialize a new instance of the hash_verify_view::iterator
         * class.
         * @param begin The start of the input range to hash.
         * @param end The end of the input range.
         * @param target_hash_size The size of the hash to create. May be 
         * bigger if the size of the output type causes it to grow. Zero gives
         * largest size available. If the output type is too large, the hash 
         * may not be storable and an exception will be thrown when that point
         * in the output range is reached.
         */
        hash_iterator(std::ranges::const_iterator_t<R> begin, std::ranges::const_sentinel_t<R> end, size_t target_hash_size)
            : hash_{ std::make_unique<hash_processor_t>(target_hash_size) }
            , to_hash_current_(std::move(begin))
            , to_hash_end_(std::move(end))
            , value_{ hash_->template process<T>([this]() -> std::tuple<bool, uint8_t> { return next_byte(); }) }
        {
        }

        hash_iterator(hash_iterator<R, T, A, S, IS>& o) noexcept
            : hash_{ nullptr } // only one can hash at a time
            , to_hash_current_{o.to_hash_current_}
            , to_hash_end_{ o.to_hash_end_ }
            , value_{ o.value_ }
        {
        }
        hash_iterator(hash_iterator<R, T, A, S, IS>&&) noexcept = default;
        ~hash_iterator() = default;
        auto operator=(hash_iterator<R, T, A, S, IS> const& o) noexcept -> hash_iterator&
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

        /**
         * Increment the iterator.
         * @return The pre-incremented iterator value.
         */
        auto operator++(int) -> hash_iterator&
        {
            verify_can_hash();
            auto ret{ *this };
            value_ = hash_->template process<T>([this]() -> std::tuple<bool, uint8_t> { return next_byte(); });
            return ret;
        }

        /**
         * Increment the iterator.
         * @return The incremented iterator value.
         */
        auto operator++() -> hash_iterator&
        {
            verify_can_hash();
            value_ = hash_->template process<T>([this]() -> std::tuple<bool, uint8_t> { return next_byte(); });
            return *this;
        }

        /**
         * Compare the provided iterator for equality.
         * @param i The iterator to compare against.
         * @return True if the provided iterator is the same as this one.
         */
        auto equals(const hash_iterator& i) const noexcept -> bool
        {
            if constexpr (sizeof(input_type) == 1)
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
            else
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
        }

        /**
         * Compare the provided sentinel for equality.
         * @return True if at the end of the hashed view.
         */
        auto equals(const hash_sentinel<R, T, A, S, IS>&) const noexcept -> bool
        {
            verify_can_hash();
            return hash_->complete();
        }

        /**
         * Gets the current hashed value.
         * @return The current hashed value.
         */
        auto operator*() const -> output_type
        {
            return value_;
        }

        auto operator==(const hash_iterator& other) const noexcept -> bool { return equals(other); }
        auto operator==(const hash_sentinel<R, T, A, S, IS>& s) const noexcept -> bool { return equals(s); }
        auto operator!=(const hash_iterator& other) const noexcept -> bool { return !equals(other); }
        auto operator!=(const hash_sentinel<R, T, A, S, IS>& s) const noexcept -> bool { return !equals(s); }

    private:
        static auto input_init()
        {
            if constexpr (sizeof(input_type) == 1)
            {
                return empty{};
            }
            else
            {
                return input_value_with_position{ {}, sizeof(input_type) };
            }
        }

        auto verify_can_hash() const -> void
        {
            if (!hash_)
            {
                throw std::runtime_error("Only one copy of the hash iterator can hash. You probably made a copy of the iterator and tried to use it. Moving the iterator is fine.");
            }
        }

        auto input_complete() -> bool
        {
            if constexpr (sizeof(input_type) == 1)
            {
                return to_hash_current_ == to_hash_end_;
            }
            else
            {
                return input_.position == sizeof(input_type) && to_hash_current_ == to_hash_end_;
            }
        }

        auto next_byte_from_input_range() -> std::tuple<bool, uint8_t>
        {
            if constexpr (sizeof(input_type) == 1)
            {
                if (input_complete())
                {
                    return { false, static_cast<uint8_t>(0) };
                }

                return { true, *to_hash_current_++ };
            }
            else
            {
                if (input_complete())
                {
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
        }

        auto next_byte() -> std::tuple<bool, uint8_t>
        {
            if constexpr (IS == iterate_style::skip_appended_hash)
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
            else
            {
                return next_byte_from_input_range();
            }
        }
    };

    template<std::ranges::viewable_range R, typename T, sph::hash_algorithm A, sph::hash_style S, iterate_style IS>
        requires std::ranges::input_range<R>&& std::is_standard_layout_v<T>&& std::is_standard_layout_v<std::remove_cvref_t<std::ranges::range_value_t<R>>>
    struct hash_sentinel
    {
        auto operator==(const hash_sentinel& /*other*/) const noexcept -> bool { return true; }
        auto operator==(const hash_iterator<R, T, A, S, IS>& i) const noexcept -> bool { return i.equals(*this); }
        auto operator!=(const hash_sentinel& /*other*/) const noexcept -> bool { return false; }
        auto operator!=(const hash_iterator<R, T, A, S, IS>& i) const noexcept -> bool { return !i.equals(*this); }
    };
}
