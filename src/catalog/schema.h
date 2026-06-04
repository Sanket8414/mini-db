#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "common/types.h"

// Describes one column in a table
struct ColumnSchema {
    std::string name;
    TypeId      type;
    uint32_t    max_length;    // only used for VARCHAR
    bool        is_primary_key;

    ColumnSchema() : type(TypeId::INVALID), max_length(0), is_primary_key(false) {}
    ColumnSchema(const std::string &name, TypeId type,
                 uint32_t max_length = 0, bool is_pk = false)
        : name(name), type(type), max_length(max_length), is_primary_key(is_pk) {}
};

// Describes a full table — its name, columns, and where its
// B+ Tree root lives on disk
struct TableSchema {
    std::string              name;
    std::vector<ColumnSchema> columns;
    uint32_t                 root_page_id;   // B+ Tree root
    uint32_t                 first_page_id;  // first heap page

    TableSchema() : root_page_id(UINT32_MAX), first_page_id(UINT32_MAX) {}
    TableSchema(const std::string &name,
                const std::vector<ColumnSchema> &cols)
        : name(name), columns(cols),
          root_page_id(UINT32_MAX), first_page_id(UINT32_MAX) {}

    // Returns index of primary key column, -1 if none
    int GetPrimaryKeyIndex() const {
        for (int i = 0; i < (int)columns.size(); i++) {
            if (columns[i].is_primary_key) return i;
        }
        return -1;
    }

    // Returns column index by name, -1 if not found
    int GetColumnIndex(const std::string &col_name) const {
        for (int i = 0; i < (int)columns.size(); i++) {
            if (columns[i].name == col_name) return i;
        }
        return -1;
    }
};