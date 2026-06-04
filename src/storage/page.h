#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "common/config.h"
#include "common/rid.h"

enum class PageType : uint8_t {
    INVALID        = 0,
    HEAP           = 1,
    META           = 2,
    BTREE_INTERNAL = 3,
    BTREE_LEAF     = 4,
};

#pragma pack(push, 1)
struct PageHeader {
    uint32_t page_id;
    PageType page_type;
    uint16_t num_slots;
    uint16_t free_space_ptr;
    uint32_t next_page_id;
};
#pragma pack(pop)

static constexpr uint16_t PAGE_HEADER_SIZE = sizeof(PageHeader);

#pragma pack(push, 1)
struct Slot {
    uint16_t offset;
    uint16_t length;
    bool     in_use;
    uint8_t  _pad;
};
#pragma pack(pop)

static constexpr uint16_t SLOT_SIZE = sizeof(Slot);

class Page {
public:
    char data[PAGE_SIZE];

    Page();
    void Init(uint32_t page_id, PageType type);

    uint16_t    InsertTuple(const char *tuple_data, uint16_t tuple_size);
    const char *GetTuple(uint16_t slot_id, uint16_t &out_size) const;
    void        DeleteTuple(uint16_t slot_id);
    uint16_t    GetFreeSpace() const;

    uint32_t GetPageId()     const;
    PageType GetPageType()   const;
    uint16_t GetNumSlots()   const;
    uint32_t GetNextPageId() const;

    void SetPageId(uint32_t page_id);
    void SetPageType(PageType type);
    void SetNextPageId(uint32_t next_page_id);

private:
    PageHeader       *Header();
    const PageHeader *Header() const;
    Slot             *GetSlot(uint16_t slot_id);
    const Slot       *GetSlot(uint16_t slot_id) const;
    uint16_t          SlotArrayOffset() const;
    uint16_t          NextSlotOffset() const;
};