#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>

enum class TypeId : uint8_t {
    INVALID  = 0,
    INT      = 1,
    VARCHAR  = 2,
    NUll_VAL = 3,
};

inline std::string TypeIdToString(TypeId type) {
    switch (type) {
        case TypeId::INT:      return "INT";
        case TypeId::VARCHAR:  return "VARCHAR";
        case TypeId::NUll_VAL: return "NULL";
        default:               return "INVALID";
    }
}

inline TypeId StringToTypeId(const std::string &str) {
    if (str == "INT")     return TypeId::INT;
    if (str == "VARCHAR") return TypeId::VARCHAR;
    if (str == "NULL")    return TypeId::NUll_VAL;
    throw std::runtime_error("Unknown type: " + str);
}