#pragma once

#include "execution/executor.h"
#include "storage/buffer_pool.h"
#include "catalog/catalog.h"
#include "index/b_plus_tree.h"
#include "common/value.h"
#include <vector>

// IndexScan — uses the B+ Tree to find tuples matching a key condition
// Supports exact match and range scan
class IndexScan : public Executor {
public:
    // Exact match: WHERE pk = value
    IndexScan(const std::string &table_name,
              Catalog           &catalog,
              BufferPool        &bp,
              const Value       &search_key);

    // Range scan: WHERE pk >= low AND pk <= high
    IndexScan(const std::string &table_name,
              Catalog           &catalog,
              BufferPool        &bp,
              const Value       &low_key,
              const Value       &high_key);

    void   Open()  override;
    Tuple *Next()  override;
    void   Close() override;

    const TableSchema &GetSchema() const { return schema_; }

private:
    std::string  table_name_;
    Catalog     &catalog_;
    BufferPool  &bp_;
    TableSchema  schema_;

    Value              low_key_;
    Value              high_key_;
    bool               is_range_scan_;

    std::vector<RID>   rids_;       // results from B+ Tree
    size_t             rid_index_;  // current position in rids_

    Tuple FetchTupleByRID(const RID &rid);
};