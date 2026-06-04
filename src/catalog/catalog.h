#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include "catalog/schema.h"
#include "storage/buffer_pool.h"
#include "common/config.h"

// Catalog stores all table metadata in memory and persists
// it to page 0 (CATALOG_PAGE_ID) of the .db file.
//
// Serialization format on page 0:
//   [num_tables: uint32_t]
//   For each table:
//     [name_len: uint16_t][name: chars]
//     [root_page_id: uint32_t]
//     [first_page_id: uint32_t]
//     [num_columns: uint16_t]
//     For each column:
//       [col_name_len: uint16_t][col_name: chars]
//       [type: uint8_t]
//       [max_length: uint32_t]
//       [is_primary_key: uint8_t]

class Catalog {
public:
    explicit Catalog(BufferPool &bp);

    // Load catalog from disk (call on startup after opening DB file)
    void Load();

    // Persist catalog to disk (call on shutdown or after schema changes)
    void Save();

    // Create a new table — throws if table already exists
    void CreateTable(const TableSchema &schema);

    // Get a table by name — throws if not found
    TableSchema &GetTable(const std::string &name);

    // Check if a table exists
    bool TableExists(const std::string &name) const;

    // List all table names
    std::vector<std::string> ListTables() const;

    // Update a table's schema (e.g. after setting root_page_id)
    void UpdateTable(const TableSchema &schema);

private:
    BufferPool &bp_;
    std::unordered_map<std::string, TableSchema> tables_;

    // Serialize all tables into a byte buffer
    std::vector<char> Serialize() const;

    // Deserialize tables from a byte buffer
    void Deserialize(const char *data, size_t size);

    // Write a uint16_t into buf at offset, advance offset
    void WriteUInt16(std::vector<char> &buf, uint16_t val) const;
    void WriteUInt32(std::vector<char> &buf, uint32_t val) const;
    void WriteUInt8 (std::vector<char> &buf, uint8_t  val) const;
    void WriteString(std::vector<char> &buf, const std::string &s) const;

    uint16_t    ReadUInt16(const char *data, size_t &offset) const;
    uint32_t    ReadUInt32(const char *data, size_t &offset) const;
    uint8_t     ReadUInt8 (const char *data, size_t &offset) const;
    std::string ReadString(const char *data, size_t &offset) const;
};