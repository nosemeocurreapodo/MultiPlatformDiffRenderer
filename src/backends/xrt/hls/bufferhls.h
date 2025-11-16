#pragma once

template <typename T>
class BufferRAM
{
public:
    BufferRAM() = default;

    BufferRAM(unsigned int n, T *src)
    {
        data_ = src;
        size_ = n;
    }

    ~BufferRAM() = default;

    // -------- capacity / info --------
    unsigned int size() const noexcept { return size_; }

    T &operator[](unsigned int i) noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

    const T &operator[](unsigned int i) const noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

private:
    T *data_;
    unsigned int size_;
};

template <typename T, int max_size>
class BufferBRAM
{
public:
    BufferBRAM() = default;

    BufferBRAM(unsigned int n)
    {
#ifndef USE_VITIS
        assert(n < max_size);
#endif
        size_ = n;
    }

    BufferBRAM(unsigned int n, const T *src) : BufferBRAM(n)
    {
    bufferhls_copy_data_loop:
        for (int i = 0; i < n; i++)
        {
#pragma HLS loop_tripcount min = max_size max = max_size avg = max_size

            data_[i] = src[i];
        }
    }

    ~BufferBRAM() = default;

    // -------- capacity / info --------
    unsigned int size() const noexcept { return size_; }

    T &operator[](unsigned int i) noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

    const T &operator[](unsigned int i) const noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

private:
    T data_[max_size];
    unsigned int size_;
};