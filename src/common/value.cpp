#include "common/value.h"
#include <stdexcept>

Value::Value() : type_(TypeId::NUll_VAL), data_(nullptr) {}

Value::Value(int32_t val) : type_(TypeId::INT), data_(val) {}

Value::Value(const std::string &val) : type_(TypeId::VARCHAR), data_(val) {}

Value::Value(std::string &&val) : type_(TypeId::VARCHAR), data_(std::move(val)) {}

bool Value::IsNull()   const { return type_ == TypeId::NUll_VAL; }
bool Value::IsInt()    const { return type_ == TypeId::INT; }
bool Value::IsString() const { return type_ == TypeId::VARCHAR; }

TypeId Value::GetType() const { return type_; }

int32_t Value::GetInt() const {
    if (!IsInt())
        throw std::runtime_error("Value::GetInt called on non-INT value");
    return std::get<int32_t>(data_);
}

const std::string &Value::GetString() const {
    if (!IsString())
        throw std::runtime_error("Value::GetString called on non-VARCHAR value");
    return std::get<std::string>(data_);
}

bool Value::operator==(const Value &other) const {
    if (IsNull() || other.IsNull()) return false;
    if (type_ != other.type_) return false;
    return data_ == other.data_;
}

bool Value::operator!=(const Value &other) const {
    return !(*this == other);
}

bool Value::operator<(const Value &other) const {
    if (IsNull() || other.IsNull()) return false;
    if (type_ != other.type_)
        throw std::runtime_error("Cannot compare values of different types");
    if (IsInt())    return GetInt() < other.GetInt();
    if (IsString()) return GetString() < other.GetString();
    return false;
}

bool Value::operator>(const Value &other) const {
    return other < *this;
}

bool Value::operator<=(const Value &other) const {
    return !(other < *this);
}

bool Value::operator>=(const Value &other) const {
    return !(*this < other);
}

std::string Value::ToString() const {
    if (IsNull())   return "NULL";
    if (IsInt())    return std::to_string(GetInt());
    if (IsString()) return "'" + GetString() + "'";
    return "INVALID";
}

std::ostream &operator<<(std::ostream &os, const Value &v) {
    os << v.ToString();
    return os;
}