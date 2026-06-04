#include "storage/disk_manager.h"
#include <stdexcept>
#include <cstring>

DiskManager::DiskManager(const std::string &db_file_path)
    : file_path_(db_file_path), num_pages_(0) {

    // Open for read+write in binary mode; create if it doesn't exist
    file_.open(db_file_path, std::ios::in | std::ios::out |
                              std::ios::binary);
    if (!file_.is_open()) {
        // File doesn't exist yet — create it, then reopen for r/w
        std::fstream create(db_file_path, std::ios::out | std::ios::binary);
        if (!create.is_open())
            throw std::runtime_error("DiskManager: failed to create: " + db_file_path);
        create.close();
        file_.open(db_file_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open())
            throw std::runtime_error("DiskManager: failed to open: " + db_file_path);
    }

    // Calculate number of existing pages from file size
    file_.seekg(0, std::ios::end);
    std::streamoff file_size = file_.tellg();
    num_pages_ = static_cast<uint32_t>(file_size / PAGE_SIZE);
}

DiskManager::~DiskManager() {
    if (file_.is_open()) {
        Flush();
        file_.close();
    }
}

void DiskManager::ReadPage(uint32_t page_id, char *buffer) {
    if (page_id >= num_pages_)
        throw std::runtime_error("ReadPage: page_id out of range");

    memset(buffer, 0, PAGE_SIZE);
    file_.seekg(static_cast<std::streamoff>(PageOffset(page_id)));
    file_.read(buffer, PAGE_SIZE);
    // Short read on the last page is fine — buffer is pre-zeroed above
}

void DiskManager::WritePage(uint32_t page_id, const char *buffer) {
    if (page_id >= num_pages_)
        throw std::runtime_error("WritePage: page_id out of range, use AllocatePage first");

    file_.seekp(static_cast<std::streamoff>(PageOffset(page_id)));
    file_.write(buffer, PAGE_SIZE);
}

uint32_t DiskManager::AllocatePage() {
    uint32_t new_page_id = num_pages_;
    char empty[PAGE_SIZE];
    memset(empty, 0, PAGE_SIZE);

    file_.seekp(static_cast<std::streamoff>(PageOffset(new_page_id)));
    file_.write(empty, PAGE_SIZE);
    num_pages_++;
    return new_page_id;
}

void DiskManager::Flush() {
    file_.flush();
}

uint64_t DiskManager::PageOffset(uint32_t page_id) const {
    return static_cast<uint64_t>(page_id) * PAGE_SIZE;
}