#include "execution/seq_scan.h"
#include <stdexcept>

SeqScan::SeqScan(const std::string &table_name,
                 Catalog           &catalog,
                 BufferPool        &bp)
    : table_name_(table_name),
      catalog_(catalog),
      bp_(bp),
      cur_page_id_(INVALID_PAGE_ID),
      cur_slot_id_(0),
      done_(false) {}

void SeqScan::Open() {
    schema_      = catalog_.GetTable(table_name_);
    cur_page_id_ = schema_.first_page_id;
    cur_slot_id_ = 0;
    done_        = (cur_page_id_ == INVALID_PAGE_ID);
}

Tuple *SeqScan::Next() {
    if (done_) return nullptr;

    while (cur_page_id_ != INVALID_PAGE_ID) {
        Page    *page = bp_.FetchPage(cur_page_id_);
        uint16_t num_slots = page->GetNumSlots();

        while (cur_slot_id_ < num_slots) {
            uint16_t    size = 0;
            const char *data = page->GetTuple(cur_slot_id_, size);
            cur_slot_id_++;

            if (data == nullptr) continue; // deleted slot, skip

            // Build type list from schema for deserialization
            std::vector<TypeId> types;
            for (auto &col : schema_.columns) types.push_back(col.type);

            current_tuple_ = Tuple::Deserialize(data, size, types);
            bp_.UnpinPage(cur_page_id_, false);
            return &current_tuple_;
        }

        // Move to next page
        uint32_t next = page->GetNextPageId();
        bp_.UnpinPage(cur_page_id_, false);
        cur_page_id_ = next;
        cur_slot_id_ = 0;
    }

    done_ = true;
    return nullptr;
}

void SeqScan::Close() {
    done_ = true;
}