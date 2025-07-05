#pragma once
#include <array>
#include <ranges>
#include <sph/hash_style.h>
#include <sph/ranges/views/detail/process_util.h>

namespace sph::ranges::views::detail::concat
{
    /**
     * @brief A range that concatenates two contiguous ranges of uint8_t.
     *
     * This range provides a single iterator that iterates over the elements
     * of the first range followed by the elements of the second range.
     *
     * @tparam R1 The type of the first range.
     * @tparam R2 The type of the second range.
     */
    template <std::ranges::contiguous_range R1, std::ranges::contiguous_range R2>
        requires std::same_as<std::ranges::range_value_t<R1>, uint8_t>&&
    std::same_as<std::ranges::range_value_t<R2>, uint8_t>
        class first_second_iterator
    {
        using Iter1 = std::ranges::iterator_t<R1>;
        using Iter2 = std::ranges::iterator_t<R2>;

        Iter1 first_;
        Iter1 first_end_;
        Iter2 second_;
        Iter2 second_end_;
        // 0 = in first, 1 = in second, 2 = end
        int which_;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = uint8_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const uint8_t*;
        using reference = const uint8_t&;

        first_second_iterator(Iter1 first, Iter1 first_end, Iter2 second, Iter2 second_end, int which)
            : first_(first), first_end_(first_end), second_(second), second_end_(second_end), which_(which) {
        }

        reference operator*() const {
            return which_ == 0 ? *first_ : *second_;
        }

        first_second_iterator& operator++() {
            if (which_ == 0) {
                ++first_;
                if (first_ == first_end_) {
                    which_ = 1;
                }
            }
            if (which_ == 1) {
                ++second_;
                if (second_ == second_end_) {
                    which_ = 2;
                }
            }
            return *this;
        }

        bool operator==(const first_second_iterator& other) const {
            return which_ == other.which_ &&
                ((which_ == 0 && first_ == other.first_) ||
                    (which_ == 1 && second_ == other.second_) ||
                    (which_ == 2));
        }
        bool operator!=(const first_second_iterator& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief A range that concatenates two contiguous ranges of uint8_t.
     *
     * This range provides a single iterator that iterates over the elements
     * of the first range followed by the elements of the second range.
     *
     * @tparam R1 The type of the first range.
     * @tparam R2 The type of the second range.
     */
    template <std::ranges::contiguous_range R1, std::ranges::contiguous_range R2>
        requires std::same_as<std::ranges::range_value_t<R1>, uint8_t>&&
    std::same_as<std::ranges::range_value_t<R2>, uint8_t>
        class first_second_range
    {
        R1 first_;
        R2 second_;

    public:
        first_second_range(R1 first, R2 second)
            : first_(std::move(first)), second_(std::move(second)) {
        }

        auto begin() const {
            return first_second_iterator<R1, R2>(
                std::ranges::begin(first_), std::ranges::end(first_),
                std::ranges::begin(second_), std::ranges::end(second_),
                first_.empty() ? 1 : 0
            );
        }
        auto end() const {
            return first_second_iterator<R1, R2>(
                std::ranges::end(first_), std::ranges::end(first_),
                std::ranges::end(second_), std::ranges::end(second_),
                2
            );
        }
    };
}

namespace sph::ranges::views::detail
{
    template<typename O, basic_hash H>
    class padded_hash
    {
        H hash_;
        size_t target_hash_size_;
        std::array<uint8_t, sizeof(O)> pad_buffer_{ create_pad_array() };
    public:
        padded_hash(size_t hash_size)
            : hash_{ hash_size }
            , target_hash_size_ {hash_.size() + 1}
        {
        }

        auto hash() const -> concat::first_second_range<
            decltype(hash_.hash()),
            std::span<const uint8_t>
        >
        {
            auto hash_range = hash_.hash();
            std::span<const uint8_t> pad_span{ pad_buffer_.data(), target_hash_size_ - hash_.target_hash_size() };
            return concat::first_second_range(hash_range, pad_span);
        }

        /**
         * \brief Sets the target hash size.
         *
         * Target hash size must be between the target hash size of the hash
         * algorithm plus 1 and the target hash size of the hash algorithm plus
         * the size of O.
         */
        auto set_target_hash_size(size_t length) -> void
        {
            if (length > hash_.target_hash_size() + sizeof(O))
            {
                throw std::invalid_argument(std::format("Length {} is larger than maximum padded hash size {}", length, hash_.target_hash_size() + sizeof(O)));
            }

            target_hash_size_ = length;
        }

        /**
         * \brief Gets the length of the hash including padding in bytes.
         */
        auto target_hash_size() const -> size_t
        {
            return target_hash_size_;
        }

        auto update(std::span<uint8_t const, H::chunk_size> const& chunk) -> void
        {
            hash_.update(chunk);
        }
    private:
        /**
         * \brief Creates an array of size sizeof(O) with the first element set
         * to 0x80.
         * 
         * This is the standard padding for hashes, where the first byte is set
         * to 0x80 from RFC1321 (although not used in the exact same way).
         */
        constexpr auto create_pad_array() -> std::array<uint8_t, sizeof(O)> {
            std::array<uint8_t, sizeof(O)> ret{};
            ret[0] = 0x80; // Set the first element to 0x80
            return ret;    // The rest remain 0x00 due to value initialization
        }
    };
}
