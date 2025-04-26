#pragma once
namespace sph::ranges::views::detail
{
    template<typename T, sph::hash_algorithm A>
    class rolling_buffer
    {
        std::array<uint8_t, sph::hash_param<A>::hash_size> buf_;
        size_t end_{ 0 };
        size_t data_current_{ 0 };
        size_t data_end_{ std::numeric_limits<size_t>::max() };
    public:
        auto next(uint8_t v) -> std::optional<uint8_t>
        {
            auto i{ end_ % buf_.size() };
            uint8_t ret{ buf_[i] };
            buf_[i] = v;
            ++end_;
            return end_ > buf_.size() ? std::optional<uint8_t>{} : ret;
        }
        auto next() -> std::tuple<bool, uint8_t>
        {
            if (!done())
            {
                throw std::runtime_error("bad call to rolling_buffer::next(). done(element_size, target_hash_size) was not called yet.");
            }

            if (data_current_ == data_end_)
            {
                return { false, 0 };
            }

            auto i{ data_current_ % buf_.size() };
            uint8_t ret{ buf_[i] };
            ++data_current_;
            return { true, ret };
        }

        auto hash_positions() -> std::tuple<size_t, size_t>
        {
            if (!done())
            {
                throw std::runtime_error("bad call to rolling_buffer::hash_positions(). done(element_size, target_hash_size) was not called yet.");
            }

            return { data_end_, end_ };
        }
        auto operator[](size_t i) const -> uint8_t { return buf_[(end_ + i + 1) % buf_.size()]; }
        auto size() const -> size_t { return buf_.size(); }
        auto done() const -> bool { return data_end_ != std::numeric_limits<size_t>::max(); }
        auto done(size_t target_hash_size) -> void
        {
            if (end_ < target_hash_size)
            {
                throw std::runtime_error(std::format("Truncated data. Expected {} at least bytes, only {} bytes available.", target_hash_size, end_));
            }

            data_end_ = (((end_ - target_hash_size) / sizeof(T)) * sizeof(T));
            if (end_ - data_end_ > buf_.size())
            {
                throw std::runtime_error(std::format("Truncated data or bad element size. Cannot fit at least {} {}-sized elements along with at least {} bytes of hash in {} bytes.",
                    (data_end_ / sizeof(T)) + 1, sizeof(T), target_hash_size, end_));
            }

            data_current_ = end_ > buf_.size() ? end_ - buf_.size() : 0;
        }
    };
}