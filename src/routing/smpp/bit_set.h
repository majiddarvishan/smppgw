#pragma once

#include <array>
#include <string>
#include <vector>

/**
 * @brief simple bit_set that uses uint64_t as underlaying data type
 */
class bit_set
{
public:
    /**
     * @brief bit_set construct a bit_set with t_size bits
     * @param t_size
     */
    bit_set(size_t t_size);
    ~bit_set();
    bit_set(const bit_set& otherbit_set);
    bit_set(bit_set&& otherbit_set) noexcept;
    void operator&=(const bit_set& rhs);
    void operator|=(const bit_set& rhs);
    bool operator==(const bit_set& rhs) const;
    /**
     * @brief
     * @param rhs
     * @return true, if set bits of cond is set already
     */
    bool check_set_flags(const bit_set& cond) const;

    /**
     * @brief
     * @param rhs
     * @return true, if set bits of cond is unset already
     */
    bool check_unset_flags(const bit_set& cond) const;

    /**
     * @brief shift bit_set to right just one bit
     */
    void shift_right();
    void set(const size_t ind, bool value = true);
    bool get(size_t ind) const;
    std::string to_string() const;

    /**
     * @brief Uses an optimized algorithm (with gcc) to return set indices
     * @return
     */
    std::vector<size_t> get_set_bits_indices() const;

private:
    uint64_t* storage_ = nullptr;
    size_t word_size_;
    size_t size_;
    static const std::array<uint64_t, 64> ind_set_;
    static const std::array<uint64_t, 64> ind_unset_;
};
