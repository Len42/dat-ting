#pragma once

#include <utility>
#include <cstddef>

template<typename T, size_t CAPACITY>
class RingBuf
{
public:
    using value_type = T;
    using size_type = size_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;

protected:
    using this_t = RingBuf<T, CAPACITY>;
    
    template<typename RINGBUF_T, typename VALUE_T, typename REFVAL_T>
    class ForwardIterBase;

    template<typename RINGBUF_T, typename VALUE_T, typename REFVAL_T>
    friend class ForwardIterBase;

public:
    using iterator = ForwardIterBase<this_t, value_type, value_type>;
    using const_iterator = ForwardIterBase<const this_t, value_type, const value_type>;

public:
    constexpr bool empty() const noexcept { return read == write; }

    constexpr bool full() const noexcept { return ((write + 1) % bufSize) == read; }

    constexpr size_t size() const noexcept {
        if (write >= read)
            return write - read;
        else
            return write + bufSize - read;
    }

    constexpr size_t max_size() const noexcept { return bufCapacity; }

    constexpr void clear() noexcept { read = write = 0; }

    constexpr void shrink(size_t smallerSize) noexcept {
        while (size() > smallerSize) pop();
    }

    constexpr void push(const T& val) noexcept {
        if (full()) pop();
        push_if_room(val);
    }

    constexpr void push(T&& val) noexcept {
        if (full()) pop();
        push_if_room(std::move(val));
    }

    constexpr void push_if_room(const T& val) noexcept {
        // There's no error handling, so if full just do nothing.
        // (Do not pop() to make room because that messes up concurrency.)
        if (!full()) {
            buf[write] = val;
            write = (write + 1) % bufSize;
        }
    }

    constexpr void push_if_room(T&& val) noexcept {
        // There's no error handling, so if full just do nothing.
        // (Do not pop() to make room because that messes up concurrency.)
        if (!full()) {
            buf[write] = std::move(val);
            write = (write + 1) % bufSize;
        }
    }

    constexpr value_type pop() noexcept {
        // There's no error handling, so if empty just return a null/default value.
        if (empty()) {
            return dummyVal;
        } else {
            size_t r = read;
            read = (read + 1) % bufSize;
            return std::move(buf[r]);
        }
    }

    constexpr reference front() noexcept {
        if (empty()) {
            return dummyVal;
        } else {
            return buf[read];
        }
    }

    constexpr const_reference front() const noexcept {
        if (empty()) {
            return dummyVal;
        } else {
            return buf[read];
        }
    }

    constexpr reference back() noexcept {
        if (empty()) {
            return dummyVal;
        } else {
            size_t w = (write == 0) ? bufSize - 1 : write - 1;
            return buf[w];
        }
    }

    constexpr const_reference back() const noexcept {
        if (empty()) {
            return dummyVal;
        } else {
            size_t w = (write == 0) ? bufSize - 1 : write - 1;
            return buf[w];
        }
    }

    constexpr iterator begin() noexcept { return iterator(this, read); }

    constexpr iterator end() noexcept { return iterator(this, write); }

    constexpr const_iterator begin() const noexcept { return const_iterator(this, read); }

    constexpr const_iterator end() const noexcept { return const_iterator(this, write); }

    constexpr iterator cbegin() const noexcept { return begin(); }

    constexpr iterator cend() const noexcept { return end(); }

protected:
    static constexpr size_t bufCapacity = CAPACITY;

    static constexpr size_t bufSize = bufCapacity + 1;

    T buf[bufSize] = { };

    size_t read = 0;

    size_t write = 0;

    value_type dummyVal{ };

protected:
    template<typename RINGBUF_T, typename VALUE_T, typename REFVAL_T>
    class ForwardIterBase
    {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = VALUE_T;
        using pointer = REFVAL_T*;
        using reference = REFVAL_T&;

        ForwardIterBase() = default;

        ForwardIterBase(RINGBUF_T* b, size_t p) : buf(b), pos(p) { }

        reference operator*() const noexcept { return buf->buf[pos]; }

        pointer operator->() const noexcept { return &buf->buf[pos]; }

        ForwardIterBase& operator++() noexcept {
            if (buf) {
                pos = (pos + 1) % buf->bufSize;
            }
            return *this;
        }

        ForwardIterBase operator++(int) noexcept {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        bool operator==(const ForwardIterBase& other) const noexcept {
            return buf == other.buf && pos == other.pos;
        }

    protected:
        RINGBUF_T* buf = nullptr;

        size_t pos = 0;
    };
};

template<typename T, size_t NUM_SAMPLES>
class RunningAverage
{
public:
    constexpr T getAverage() const { return average; }

    constexpr T update(T newVal) {
        if (buf.full()) {
            T oldVal = buf.front();
            buf.push(newVal);
            average += float(newVal - oldVal) / buf.size();
        } else {
            buf.push(newVal);
            average += float(newVal - average) / buf.size();
        }
        return static_cast<T>(average);
    }
    
private:
    RingBuf<T, NUM_SAMPLES> buf{};

    float average = 0;
};
