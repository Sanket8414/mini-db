#include "execution/index_scan.h"
#include <stdexcept>

IndexScan::IndexScan(const std::string &table_name,
                     Catalog           &catalog,
                     BufferPool        &bp,
                     const Value       &search_key)
    : table_name_(table_name),
      catalog_(catalog),
      bp_(bp),
      low_key_(search_key),
      high_key_(search_key),
      is_range_scan_(false),
      rid_index_(0) {}

IndexScan::IndexScan(const std::string &table_name,
                     Catalog           &catalog,
                     BufferPool        &bp,
                     const Value       &low_key,
                     const Value       &high_key)
    : table_name_(table_name),
      catalog_(catalog),
      bp_(bp),
      low_key_(low_key),
      high_key_(high_key),
      is_range_scan_(true),
      rid_index_(0) {}

void IndexScan::Open() {
    schema_ = catalog_.GetTable(table_name_);

    BPlusTree tree(bp_, schema_.root_page_id);

    if (is_range_scan_) {
        rids_ = tree.RangeScan(low_key_.GetInt(), high_key_.GetInt());
    } else {
        auto result = tree.Search(low_key_.GetInt());
        if (result.has_value()) rids_.push_back(result.value());
    }

    rid_index_ = 0;
}

Tuple *IndexScan::Next() {
    if (rid_index_ >= rids_.size()) return nullptr;
    current_tuple_ = FetchTupleByRID(rids_[rid_index_++]);
    return &current_tuple_;
}

void IndexScan::Close() {
    rids_.clear();
    rid_index_ = 0;
}

Tuple IndexScan::FetchTupleByRID(const RID &rid) {
    Page    *page = bp_.FetchPage(rid.page_id);
    uint16_t size = 0;
    const char *data = page->GetTuple(rid.slot_id, size);

    if (data == nullptr) {
        bp_.UnpinPage(rid.page_id, false);
        throw std::runtime_error("IndexScan: tuple not found at RID "
                                 + rid.ToString());
    }

    std::vector<TypeId> types;
    for (auto &col : schema_.columns) types.push_back(col.type);

    Tuple t = Tuple::Deserialize(data, size, types);
    bp_.UnpinPage(rid.page_id, false);
    return t;
}