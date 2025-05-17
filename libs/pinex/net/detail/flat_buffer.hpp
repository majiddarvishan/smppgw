#pragma once

#include <boost/asio/buffer.hpp>

#include <algorithm>
#include <array>

namespace pa::pinex::detail
{
template<typename T, std::size_t N>
class flat_buffer
{
    std::array<T, N> buf_{};

    T* in_{buf_.begin()};
    T* out_{buf_.begin()};
    T* last_{buf_.begin()};

  public:
    flat_buffer() noexcept = default;

    flat_buffer(const flat_buffer& other)
    {
        this->commit(boost::asio::buffer_copy(this->prepare(other.size()), other.data()));
    }

    flat_buffer& operator=(const flat_buffer& other)
    {
        if (this == &other)
            return *this;
        this->consume(this->size());
        this->commit(boost::asio::buffer_copy(this->prepare(other.size()), other.data()));
        return *this;
    }

    flat_buffer(flat_buffer&&) = delete;
    flat_buffer& operator=(flat_buffer&&) = delete;
    ~flat_buffer() = default;

    void clear() noexcept
    {
        in_ = buf_.begin();
        out_ = buf_.begin();
        last_ = buf_.begin();
    }

    std::size_t capacity() const noexcept
    {
        return buf_.size();
    }

    boost::asio::const_buffer data() const noexcept
    {
        return {in_, dist(in_, out_)};
    }

    const T* begin() const noexcept
    {
        return in_;
    }

    const T* end() const noexcept
    {
        return out_;
    }

    std::size_t size() const noexcept
    {
        return dist(in_, out_);
    }

    boost::asio::mutable_buffer prepare(std::size_t n)
    {
        if (n <= dist(out_, buf_.end()))
        {
            last_ = out_ + n;
            return {out_, n};
        }
        const auto len = size();
        if (n > capacity() - len)
            throw(std::length_error{"flat_buffer::prepare buffer overflow"});
        if (len > 0)
            std::memmove(buf_.begin(), in_, len);
        in_ = buf_.begin();
        out_ = in_ + len;
        last_ = out_ + n;
        return {out_, n};
    }

    void commit(std::size_t n) noexcept
    {
        out_ += (std::min<std::size_t>)(n, last_ - out_);
    }

    void consume(std::size_t n) noexcept
    {
        if (n >= size())
        {
            in_ = buf_.begin();
            out_ = in_;
            return;
        }
        in_ += n;
    }

  private:
    static std::size_t dist(const T* first, const T* last) noexcept
    {
        return static_cast<std::size_t>(last - first);
    }
};
}  // namespace pa::pinex::detail