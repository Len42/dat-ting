#pragma once

#include <utility>
#include <cstddef>

/// @brief Ring buffer (circular buffer) with fixed capacity
/// @tparam T 
/// @tparam CAPACITY 
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
    /// @brief Check if the buffer is empty
    /// @return 
    constexpr bool empty() const noexcept { return read == write; }

    /// @brief Check if the buffer is full
    /// @return 
    constexpr bool full() const noexcept { return ((write + 1) % bufSize) == read; }

    /// @brief Return the number of elements in the buffer
    /// @return 
    constexpr size_t size() const noexcept {
        if (write >= read)
            return write - read;
        else
            return write + bufSize - read;
    }

    /// @brief Return the maximum number of elements that can be stored in the buffer
    /// @return 
    constexpr size_t max_size() const noexcept { return bufCapacity; }

    /// @brief Remove all elements from the buffer
    constexpr void clear() noexcept { read = write = 0; }

    /// @brief Remove (pop) elements until the buffer contains no more than smallerSize elements
    /// @param smallerSize 
    constexpr void shrink(size_t smallerSize) noexcept {
        while (size() > smallerSize) pop();
    }

    /// @brief Insert an element at the end of the buffer. If necessary, discard
    /// an element from the start of the buffer to make room.
    /// @param val 
    constexpr void push(const T& val) noexcept {
        if (full()) pop();
        push_if_room(val);
    }

    /// @brief Insert an element at the end of the buffer. If necessary, discard
    /// an element from the start of the buffer to make room.
    /// @param val 
    constexpr void push(T&& val) noexcept {
        if (full()) pop();
        push_if_room(std::move(val));
    }

    /// @brief Insert an element at the end of the buffer. If the buffer is full
    /// do nothing.
    /// @param val 
    constexpr void push_if_room(const T& val) noexcept {
        if (!full()) {
            buf[write] = val;
            write = (write + 1) % bufSize;
        }
    }

    /// @brief Insert an element at the end of the buffer. If the buffer is full
    /// do nothing.
    /// @param val 
    constexpr void push_if_room(T&& val) noexcept {
        if (!full()) {
            buf[write] = std::move(val);
            write = (write + 1) % bufSize;
        }
    }

    /// @brief Remove the first element in the buffer. If the buffer is empty
    /// return a default value.
    /// @return 
    constexpr value_type pop() noexcept {
        if (empty()) {
            return dummyVal;
        } else {
            size_t r = read;
            read = (read + 1) % bufSize;
            return std::move(buf[r]);
        }
    }

    /// @brief Return a reference to the first element in the buffer (without
    /// removing it). If the buffer is empty return a default element.
    /// @return 
    constexpr reference front() noexcept {
        if (empty()) {
            return dummyVal;
        } else {
            return buf[read];
        }
    }

    /// @brief Return a reference to the first element in the buffer (without
    /// removing it). If the buffer is empty return a default element.
    /// @return 
    constexpr const_reference front() const noexcept {
        if (empty()) {
            return dummyVal;
        } else {
            return buf[read];
        }
    }

    /// @brief Return a reference to the last element in the buffer (without
    /// removing it). If the buffer is empty return a default element.
    /// @return 
    constexpr reference back() noexcept {
        if (empty()) {
            return dummyVal;
        } else {
            size_t w = (write == 0) ? bufSize - 1 : write - 1;
            return buf[w];
        }
    }

    /// @brief Return a reference to the last element in the buffer (without
    /// removing it). If the buffer is empty return a default element.
    /// @return 
    constexpr const_reference back() const noexcept {
        if (empty()) {
            return dummyVal;
        } else {
            size_t w = (write == 0) ? bufSize - 1 : write - 1;
            return buf[w];
        }
    }

    /// @brief Return an iterator to the start of the buffer
    /// @return 
    constexpr iterator begin() noexcept { return iterator(this, read); }

    /// @brief Return an iterator to the end of the buffer
    /// @return 
    constexpr iterator end() noexcept { return iterator(this, write); }

    /// @brief Return an iterator to the start of the buffer
    /// @return 
    constexpr const_iterator begin() const noexcept { return const_iterator(this, read); }

    /// @brief Return an iterator to the end of the buffer
    /// @return 
    constexpr const_iterator end() const noexcept { return const_iterator(this, write); }

    /// @brief Return an iterator to the start of the buffer
    /// @return 
    constexpr iterator cbegin() const noexcept { return begin(); }

    /// @brief Return an iterator to the end of the buffer
    /// @return 
    constexpr iterator cend() const noexcept { return end(); }

protected:
    static constexpr size_t bufCapacity = CAPACITY;

    static constexpr size_t bufSize = bufCapacity + 1;

    T buf[bufSize] = { };

    size_t read = 0;

    size_t write = 0;

    value_type dummyVal{ };

protected:
    /// @brief Iterator implementation for RingBuf
    /// @tparam RINGBUF_T 
    /// @tparam VALUE_T 
    /// @tparam REFVAL_T 
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

/// @brief A running average of the most recent values in a sequence
/// @details The most recent NUM_SAMPLES values are stored in a buffer.
/// @tparam T 
/// @tparam NUM_SAMPLES 
template<typename T, size_t NUM_SAMPLES>
class RunningAverage
{
public:
    /// @brief Return the current average of the recent values
    /// @return 
    constexpr T getAverage() const { return average; }

    /// @brief Update the running average
    /// @details A new value is added to the running average and the oldest
    /// value is removed.
    /// @param newVal New value to include in the running average
    /// @return The updated average value
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
