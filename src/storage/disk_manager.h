#pragma once
#include <string>
#include <cstdint>
#include <fstream>
#include "common/config.h"

class DiskManager {
public:
    explicit DiskManager(const std::string &db_file_path);
    ~DiskManager();

    void     ReadPage(uint32_t page_id, char *buffer);
    void     WritePage(uint32_t page_id, const char *buffer);
    uint32_t AllocatePage();
    void     Flush();
    uint32_t GetNumPages() const { return num_pages_; }

private:
    std::string  file_path_;
    std::fstream file_;
    uint32_t     num_pages_;

    uint64_t PageOffset(uint32_t page_id) const;
};