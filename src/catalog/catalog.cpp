#include "catalog.h"
#include <cstring>
#include <stdexcept>

Catalog::Catalog(BufferPool &bp) : bp_(bp) {}

void Catalog::Load() {
    // FIX (bug #1): original called Deserialize before reading data_size,
    // then called it again — loading data twice from garbage bounds.
    // Now we read data_size first, then Deserialize exactly once.
    try {
        Page *page = bp_.FetchPage(CATALOG_PAGE_ID);

        uint32_t data_size = 0;
        memcpy(&data_size, page->data, sizeof(uint32_t));

        if (data_size > 0 && data_size <= PAGE_SIZE - sizeof(uint32_t)) {
            Deserialize(page->data + sizeof(uint32_t), data_size);
        }

        bp_.UnpinPage(CATALOG_PAGE_ID, false);
    } catch (...) {
        // Brand new DB — no catalog page yet, that's fine
    }
}

void Catalog::Save() {
    std::vector<char> serialized = Serialize();

    if (serialized.size() + sizeof(uint32_t) > PAGE_SIZE) {
        throw std::runtime_error("Catalog::Save — catalog too large for one page");
    }

    Page *page = bp_.FetchPage(CATALOG_PAGE_ID);

    uint32_t data_size = static_cast<uint32_t>(serialized.size());
    memcpy(page->data, &data_size, sizeof(uint32_t));
    memcpy(page->data + sizeof(uint32_t), serialized.data(), serialized.size());

    bp_.UnpinPage(CATALOG_PAGE_ID, true);
    bp_.FlushPage(CATALOG_PAGE_ID);
}

void Catalog::CreateTable(const TableSchema &schema) {
    if (TableExists(schema.name))
        throw std::runtime_error("Table already exists: " + schema.name);
    tables_[schema.name] = schema;
}

TableSchema &Catalog::GetTable(const std::string &name) {
    std::unordered_map<std::string, TableSchema>::iterator it = tables_.find(name);
    if (it == tables_.end())
        throw std::runtime_error("Table not found: " + name);
    return it->second;
}

bool Catalog::TableExists(const std::string &name) const {
    return tables_.count(name) > 0;
}

std::vector<std::string> Catalog::ListTables() const {
    std::vector<std::string> names;
    // FIX (C++17): replaced structured binding with iterator
    for (auto it = tables_.begin(); it != tables_.end(); ++it) {
        names.push_back(it->first);
    }
    return names;
}

void Catalog::UpdateTable(const TableSchema &schema) {
    if (!TableExists(schema.name))
        throw std::runtime_error("UpdateTable: table not found: " + schema.name);
    tables_[schema.name] = schema;
}

// ---- Serialization ----

std::vector<char> Catalog::Serialize() const {
    std::vector<char> buf;

    WriteUInt32(buf, static_cast<uint32_t>(tables_.size()));

    // FIX (C++17): replaced structured binding with iterator
    for (auto it = tables_.begin(); it != tables_.end(); ++it) {
        const TableSchema &schema = it->second;
        WriteString(buf, schema.name);
        WriteUInt32(buf, schema.root_page_id);
        WriteUInt32(buf, schema.first_page_id);
        WriteUInt16(buf, static_cast<uint16_t>(schema.columns.size()));

        for (size_t i = 0; i < schema.columns.size(); i++) {
            const ColumnSchema &col = schema.columns[i];
            WriteString(buf, col.name);
            WriteUInt8 (buf, static_cast<uint8_t>(col.type));
            WriteUInt32(buf, col.max_length);
            WriteUInt8 (buf, col.is_primary_key ? 1 : 0);
        }
    }

    return buf;
}

void Catalog::Deserialize(const char *data, size_t size) {
    if (size == 0) return;

    size_t   offset     = 0;
    uint32_t num_tables = ReadUInt32(data, offset);

    for (uint32_t t = 0; t < num_tables; t++) {
        TableSchema schema;
        schema.name          = ReadString(data, offset);
        schema.root_page_id  = ReadUInt32(data, offset);
        schema.first_page_id = ReadUInt32(data, offset);
        uint16_t num_cols    = ReadUInt16(data, offset);

        for (uint16_t c = 0; c < num_cols; c++) {
            ColumnSchema col;
            col.name           = ReadString(data, offset);
            col.type           = static_cast<TypeId>(ReadUInt8(data, offset));
            col.max_length     = ReadUInt32(data, offset);
            col.is_primary_key = ReadUInt8(data, offset) == 1;
            schema.columns.push_back(col);
        }

        tables_[schema.name] = schema;
    }
}

// ---- Write helpers ----

void Catalog::WriteUInt8(std::vector<char> &buf, uint8_t val) const {
    buf.push_back(static_cast<char>(val));
}

void Catalog::WriteUInt16(std::vector<char> &buf, uint16_t val) const {
    buf.push_back(static_cast<char>(val & 0xFF));
    buf.push_back(static_cast<char>((val >> 8) & 0xFF));
}

void Catalog::WriteUInt32(std::vector<char> &buf, uint32_t val) const {
    buf.push_back(static_cast<char>(val & 0xFF));
    buf.push_back(static_cast<char>((val >> 8)  & 0xFF));
    buf.push_back(static_cast<char>((val >> 16) & 0xFF));
    buf.push_back(static_cast<char>((val >> 24) & 0xFF));
}

void Catalog::WriteString(std::vector<char> &buf, const std::string &s) const {
    WriteUInt16(buf, static_cast<uint16_t>(s.size()));
    for (size_t i = 0; i < s.size(); i++) buf.push_back(s[i]);
}

// ---- Read helpers ----

uint8_t Catalog::ReadUInt8(const char *data, size_t &offset) const {
    return static_cast<uint8_t>(data[offset++]);
}

uint16_t Catalog::ReadUInt16(const char *data, size_t &offset) const {
    uint16_t val = static_cast<uint8_t>(data[offset])
                 | (static_cast<uint8_t>(data[offset + 1]) << 8);
    offset += 2;
    return val;
}

uint32_t Catalog::ReadUInt32(const char *data, size_t &offset) const {
    uint32_t val = static_cast<uint8_t>(data[offset])
                 | (static_cast<uint8_t>(data[offset + 1]) << 8)
                 | (static_cast<uint8_t>(data[offset + 2]) << 16)
                 | (static_cast<uint8_t>(data[offset + 3]) << 24);
    offset += 4;
    return val;
}

std::string Catalog::ReadString(const char *data, size_t &offset) const {
    uint16_t len = ReadUInt16(data, offset);
    std::string s(data + offset, len);
    offset += len;
    return s;
}
