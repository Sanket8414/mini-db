#pragma once

#include <vector>
#include <bitset>
#include <stdexcept>
#include "common/value.h"
#include "common/config.h"

// Maximum columns we support per table
static constexpr uint16_t MAX_COLUMNS = 64;

// Tuple — one row of data
// Internally: a vector of Values + a null bitmap
// null_bitmap[i] = 1 means column i is NULL
class Tuple {
public:
    Tuple() = default;

    explicit Tuple(const std::vector<Value> &values)
        : values_(values) {
        for (size_t i = 0; i < values.size(); i++) {
            if (values[i].IsNull()) null_bitmap_.set(i);
        }
    }

    // Get value at column index
    const Value &GetValue(uint16_t col_index) const {
        if (col_index >= values_.size())
            throw std::runtime_error("Tuple::GetValue — column index out of range");
        return values_[col_index];
    }

    // Set value at column index
    void SetValue(uint16_t col_index, const Value &val) {
        if (col_index >= values_.size())
            values_.resize(col_index + 1);
        values_[col_index] = val;
        if (val.IsNull()) null_bitmap_.set(col_index);
        else              null_bitmap_.reset(col_index);
    }

    // Is column i NULL?
    bool IsNull(uint16_t col_index) const {
        return null_bitmap_.test(col_index);
    }

    // How many columns in this tuple
    size_t Size() const { return values_.size(); }

    // Is this tuple valid (non-empty)?
    bool IsValid() const { return !values_.empty(); }

    // Serialize tuple to raw bytes for storage in a Page
    // Format: [null_bitmap: 8 bytes][col_0_data][col_1_data]...
    // INT: 4 bytes
    // VARCHAR: [len: 2 bytes][chars]
    std::vector<char> Serialize() const;

    // Deserialize from raw bytes given a schema (types needed to parse)
    static Tuple Deserialize(const char *data, uint16_t size,
                             const std::vector<TypeId> &types);

    const std::vector<Value> &GetValues() const { return values_; }

private:
    std::vector<Value>      values_;
    std::bitset<MAX_COLUMNS> null_bitmap_;
};