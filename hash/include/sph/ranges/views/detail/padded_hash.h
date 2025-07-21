#pragma once
#include <array>
#include <ranges>
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
        using const_iterator_1 = std::ranges::const_iterator_t<R1>;
        using const_iterator_2 = std::ranges::const_iterator_t<R2>;

        const_iterator_1 first_;
        const_iterator_1 first_end_;
        const_iterator_2 second_;
        const_iterator_2 second_end_;
    public:
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type = uint8_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const uint8_t*;
        using reference = const uint8_t&;

        first_second_iterator() = default;
        first_second_iterator(const_iterator_1 first, const_iterator_1 first_end, const_iterator_2 second, const_iterator_2 second_end)
            : first_(first), first_end_(first_end), second_(second), second_end_(second_end)
        {
        }

        auto operator*() const -> reference
        {
            return first_ != first_end_ ? *first_ : *second_;
        }

        auto operator++() -> first_second_iterator&
        {
            if (first_ != first_end_)
            {
                ++first_;
            }
            else if (second_ != second_end_)
            {
                ++second_;
            }

            return *this;
        }

        auto operator++(int) -> first_second_iterator
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        auto operator==(const first_second_iterator& other) const -> bool
        {
            auto my_state{ state() };
            return my_state == other.state() &&
                ((my_state == 0 && first_ == other.first_) ||
                    (my_state == 1 && second_ == other.second_) ||
                    (my_state == 2));
        }
        auto operator!=(const first_second_iterator& other) const -> bool
        {
            return !(*this == other);
        }

        private:
        auto state() const -> size_t
        {
            // 0 = in first, 1 = in second, 2 = end
            return first_ != first_end_ ? 0 : second_ != second_end_ ? 1 : 2;
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
        using iterator = first_second_iterator<R1, R2>;
        using const_iterator = iterator;
        using value_type = typename iterator::value_type;
        using difference_type = typename iterator::difference_type;
        using reference = typename iterator::reference;
        using const_reference = reference;

        first_second_range(R1 first, R2 second)
            : first_(std::move(first)), second_(std::move(second))
        {
        }

        auto begin() const -> iterator
        {
            return first_second_iterator<R1, R2>(
                std::ranges::cbegin(first_), std::ranges::cend(first_),
                std::ranges::cbegin(second_), std::ranges::cend(second_));
        }

        auto begin() -> iterator
        {
            return first_second_iterator<R1, R2>(
                std::ranges::cbegin(first_), std::ranges::cend(first_),
                std::ranges::cbegin(second_), std::ranges::cend(second_)
            );
        }

        auto end() const -> iterator
        {
            return first_second_iterator<R1, R2>(
                std::ranges::cend(first_), std::ranges::cend(first_),
                std::ranges::cend(second_), std::ranges::cend(second_)
            );
        }

        auto end() -> iterator
        {
            return first_second_iterator<R1, R2>(
                std::ranges::cend(first_), std::ranges::cend(first_),
                std::ranges::cend(second_), std::ranges::cend(second_)
            );
        }

        auto size() const -> size_t
        {
            return std::ranges::distance(first_) + std::ranges::distance(second_);
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
            , target_hash_size_ {hash_.target_hash_size() + 1}
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

        auto final(std::span<uint8_t const> const data) -> void
        {
            hash_.final(data);
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
