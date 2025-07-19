#pragma once
#include <iterator>
namespace sph::ranges::views::detail
{
    class single_bool_sentinel;

    /**
     * Provides a boolean iterator that can be used to
     * represent a single boolean value in a range.
     */
    class single_bool_iterator
    {
        bool value_;
        bool done_{ false };
    public:
        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = bool;
        using pointer = value_type*;
        using reference = value_type&;
        using input_type = bool;
        using output_type = bool;
        single_bool_iterator() noexcept : value_{ false }, done_{ true } {}
        explicit single_bool_iterator(bool value) : value_{ value } {}
        single_bool_iterator(single_bool_iterator const& o) noexcept = default;
        single_bool_iterator(single_bool_iterator&& o) noexcept = default;
        ~single_bool_iterator() noexcept = default;
        auto operator=(single_bool_iterator const& o) noexcept -> single_bool_iterator&
        {
            if (&o != this)
            {
                value_ = o.value_;
                done_ = o.done_;
            }
            return *this;
        }
        auto operator=(single_bool_iterator&& o) noexcept -> single_bool_iterator&
        {
            if (&o != this)
            {
                value_ = o.value_;
                done_ = o.done_;
            }
            return *this;
        }
        auto operator++() noexcept -> single_bool_iterator&
        {
            done_ = true;
            return *this;
        }
        auto operator++(int) noexcept -> single_bool_iterator
        {
            auto temp = *this;
            ++(*this);
            return temp;
        }
        auto operator*() const noexcept -> bool { return value_; }
        auto equals(single_bool_iterator const& other) const noexcept -> bool
        {
            return done_ == other.done_ && (done_ || value_ == other.value_);
        }
        auto equals(single_bool_sentinel const& /*other*/) const noexcept -> bool
        {
            return done_;
        }
        auto operator==(const single_bool_iterator& other) const noexcept -> bool { return equals(other); }
        auto operator==(const single_bool_sentinel& other) const noexcept -> bool { return equals(other); }
        auto operator!=(const single_bool_iterator& other) const noexcept -> bool { return !equals(other); }
        auto operator!=(const single_bool_sentinel& other) const noexcept -> bool { return !equals(other); }
    };

    class single_bool_sentinel
    {
    public:
        auto operator==(const single_bool_sentinel& /*other*/) const -> bool { return true; }
        auto operator==(const single_bool_iterator& i) const -> bool { return i.equals(*this); }
        auto operator!=(const single_bool_sentinel& /*other*/) const -> bool { return false; }
        auto operator!=(const single_bool_iterator& i) const -> bool { return !i.equals(*this); }
    };
}

