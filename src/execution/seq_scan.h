#pragma once

#include "execution/executor.h"
#include "storage/buffer_pool.h"
#include "catalog/catalog.h"

// SeqScan — scans every tuple in a table sequentially
// Iterates page by page, slot by slot
class SeqScan : public Executor {
public:
    SeqScan(const std::string &table_name,
            Catalog           &catalog,
            BufferPool        &bp);

    void   Open()  override;
    Tuple *Next()  override;
    void   Close() override;

    const TableSchema &GetSchema() const { return schema_; }

private:
    std::string  table_name_;
    Catalog     &catalog_;
    BufferPool  &bp_;
    TableSchema  schema_;

    uint32_t cur_page_id_;   // page we are currently scanning
    uint16_t cur_slot_id_;   // slot within current page
    bool     done_;
};