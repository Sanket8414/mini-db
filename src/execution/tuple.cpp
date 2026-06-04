#include "execution/tuple.h"
#include <cstring>
#include <stdexcept>

std::vector<char> Tuple::Serialize() const {
    std::vector<char> buf;

    // Write null bitmap — 8 bytes (supports up to 64 columns)
    uint64_t bitmap = 0;
    for (size_t i = 0; i < values_.size(); i++) {
        if (null_bitmap_.test(i)) bitmap |= (1ULL << i);
    }
    buf.resize(buf.size() + sizeof(uint64_t));
    memcpy(buf.data() + buf.size() - sizeof(uint64_t),
           &bitmap, sizeof(uint64_t));

    // Write each value
    for (size_t i = 0; i < values_.size(); i++) {
        const Value &v = values_[i];

        if (null_bitmap_.test(i)) continue; // NULL — no bytes written

        if (v.IsInt()) {
            int32_t val = v.GetInt();
            size_t  off = buf.size();
            buf.resize(off + sizeof(int32_t));
            memcpy(buf.data() + off, &val, sizeof(int32_t));
        } else if (v.IsString()) {
            const std::string &s   = v.GetString();
            uint16_t           len = static_cast<uint16_t>(s.size());
            size_t             off = buf.size();
            buf.resize(off + sizeof(uint16_t) + len);
            memcpy(buf.data() + off, &len, sizeof(uint16_t));
            memcpy(buf.data() + off + sizeof(uint16_t), s.data(), len);
        }
    }

    return buf;
}

Tuple Tuple::Deserialize(const char *data, uint16_t size,
                         const std::vector<TypeId> &types) {
    size_t offset = 0;

    // Read null bitmap
    uint64_t bitmap = 0;
    memcpy(&bitmap, data + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    std::vector<Value> values;
    for (size_t i = 0; i < types.size(); i++) {
        if (bitmap & (1ULL << i)) {
            values.emplace_back(); // NULL
            continue;
        }

        if (types[i] == TypeId::INT) {
            int32_t val;
            memcpy(&val, data + offset, sizeof(int32_t));
            offset += sizeof(int32_t);
            values.emplace_back(val);
        } else if (types[i] == TypeId::VARCHAR) {
            uint16_t len;
            memcpy(&len, data + offset, sizeof(uint16_t));
            offset += sizeof(uint16_t);
            std::string s(data + offset, len);
            offset += len;
            values.emplace_back(s);
        } else {
            values.emplace_back(); // unknown type → NULL
        }
    }

    return Tuple(values);
}