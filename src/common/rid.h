#pragma once

#include <cstdint>
#include <string>

struct RID {
    uint32_t page_id;
    uint16_t slot_id;

    RID() : page_id(UINT32_MAX), slot_id(UINT16_MAX) {}
    RID(uint32_t page_id, uint16_t slot_id)
        : page_id(page_id), slot_id(slot_id) {}

    bool IsValid() const {
        return page_id != UINT32_MAX && slot_id != UINT16_MAX;
    }

    bool operator==(const RID &other) const {
        return page_id == other.page_id && slot_id == other.slot_id;
    }

    bool operator!=(const RID &other) const {
        return !(*this == other);
    }

    std::string ToString() const {
        return "(" + std::to_string(page_id) + ", " + std::to_string(slot_id) + ")";
    }
};