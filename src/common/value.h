#pragma once

#include <variant>
#include <string>
#include <stdexcept>
#include <iostream>
#include "common/types.h"

// A Value is one column's data in a tuple.
// It is either an int, a string, or NULL.
using ValueData = std::variant<int32_t, std::string, std::nullptr_t>;

class Value {
public:
    // Constructors
    Value();                                // NULL value
    explicit Value(int32_t val);            // INT value
    explicit Value(const std::string &val); // VARCHAR value
    explicit Value(std::string &&val);      // VARCHAR value (move)

    // Type checks
    bool IsNull()    const;
    bool IsInt()     const;
    bool IsString()  const;
    TypeId GetType() const;

    // Accessors — throw if wrong type
    int32_t            GetInt()    const;
    const std::string &GetString() const;

    // Comparison operators (for WHERE clause evaluation)
    bool operator==(const Value &other) const;
    bool operator!=(const Value &other) const;
    bool operator< (const Value &other) const;
    bool operator> (const Value &other) const;
    bool operator<=(const Value &other) const;
    bool operator>=(const Value &other) const;

    std::string ToString() const;

    friend std::ostream &operator<<(std::ostream &os, const Value &v);

private:
    TypeId    type_;
    ValueData data_;
};