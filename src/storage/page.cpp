#include "storage/page.h"
#include <cstring>
#include <stdexcept>

Page::Page() {
    memset(data, 0, PAGE_SIZE);
}

void Page::Init(uint32_t page_id, PageType type) {
    memset(data, 0, PAGE_SIZE);
    PageHeader *h     = Header();
    h->page_id        = page_id;
    h->page_type      = type;
    h->num_slots      = 0;
    h->free_space_ptr = PAGE_SIZE;
    h->next_page_id   = UINT32_MAX;
}

uint16_t Page::InsertTuple(const char *tuple_data, uint16_t tuple_size) {
    if (GetFreeSpace() < tuple_size + SLOT_SIZE) {
        throw std::runtime_error("Page is full");
    }
    PageHeader *h      = Header();
    h->free_space_ptr -= tuple_size;
    uint16_t offset    = h->free_space_ptr;
    memcpy(data + offset, tuple_data, tuple_size);

    uint16_t slot_id   = h->num_slots;
    Slot *slot         = GetSlot(slot_id);
    slot->offset       = offset;
    slot->length       = tuple_size;
    slot->in_use       = true;
    h->num_slots++;

    return slot_id;
}

const char *Page::GetTuple(uint16_t slot_id, uint16_t &out_size) const {
    if (slot_id >= Header()->num_slots) return nullptr;
    const Slot *slot = GetSlot(slot_id);
    if (!slot->in_use) return nullptr;
    out_size = slot->length;
    return data + slot->offset;
}

void Page::DeleteTuple(uint16_t slot_id) {
    if (slot_id >= Header()->num_slots)
        throw std::runtime_error("DeleteTuple: invalid slot_id");
    GetSlot(slot_id)->in_use = false;
}

uint16_t Page::GetFreeSpace() const {
    const PageHeader *h     = Header();
    uint16_t slot_array_end = SlotArrayOffset() + h->num_slots * SLOT_SIZE;
    return h->free_space_ptr - slot_array_end;
}

uint32_t Page::GetPageId()     const { return Header()->page_id; }
PageType Page::GetPageType()   const { return Header()->page_type; }
uint16_t Page::GetNumSlots()   const { return Header()->num_slots; }
uint32_t Page::GetNextPageId() const { return Header()->next_page_id; }

void Page::SetPageId(uint32_t id)      { Header()->page_id = id; }
void Page::SetPageType(PageType type)  { Header()->page_type = type; }
void Page::SetNextPageId(uint32_t id)  { Header()->next_page_id = id; }

PageHeader *Page::Header() {
    return reinterpret_cast<PageHeader *>(data);
}
const PageHeader *Page::Header() const {
    return reinterpret_cast<const PageHeader *>(data);
}
Slot *Page::GetSlot(uint16_t slot_id) {
    return reinterpret_cast<Slot *>(data + SlotArrayOffset() + slot_id * SLOT_SIZE);
}
const Slot *Page::GetSlot(uint16_t slot_id) const {
    return reinterpret_cast<const Slot *>(data + SlotArrayOffset() + slot_id * SLOT_SIZE);
}
uint16_t Page::SlotArrayOffset() const { return PAGE_HEADER_SIZE; }
uint16_t Page::NextSlotOffset()  const {
    return SlotArrayOffset() + Header()->num_slots * SLOT_SIZE;
}