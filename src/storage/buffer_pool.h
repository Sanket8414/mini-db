#pragma once

#include <unordered_map>
#include <list>
#include <vector>
#include "storage/page.h"
#include "storage/disk_manager.h"
#include "common/config.h"

struct Frame {
    Page     page;
    int      pin_count;
    bool     is_dirty;
    Frame() : pin_count(0), is_dirty(false) {}
};

class LRUReplacer {
public:
    explicit LRUReplacer(uint32_t capacity);
    void   Insert(uint32_t frame_id);
    bool   Evict(uint32_t &out_frame_id);
    void   Remove(uint32_t frame_id);
    size_t Size() const;

private:
    uint32_t capacity_;
    std::list<uint32_t> lru_list_;
    std::unordered_map<uint32_t, std::list<uint32_t>::iterator> lru_map_;
};

class BufferPool {
public:
    BufferPool(uint32_t pool_size, DiskManager &disk_manager);
    ~BufferPool();

    Page *FetchPage(uint32_t page_id);
    Page *NewPage(uint32_t &out_page_id);
    void  UnpinPage(uint32_t page_id, bool is_dirty);
    void  FlushPage(uint32_t page_id);
    void  FlushAllPages();

    uint32_t GetPoolSize() const { return pool_size_; }

private:
    uint32_t     pool_size_;
    DiskManager &disk_manager_;
    std::vector<Frame> frames_;
    std::unordered_map<uint32_t, uint32_t> page_table_;
    std::list<uint32_t> free_list_;
    LRUReplacer replacer_;

    bool GetFreeFrame(uint32_t &out_frame_id);
};