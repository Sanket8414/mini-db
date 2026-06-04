#include "storage/buffer_pool.h"
#include <cstring>
#include <stdexcept>

// ---- LRUReplacer ----

LRUReplacer::LRUReplacer(uint32_t capacity) : capacity_(capacity) {}

void LRUReplacer::Insert(uint32_t frame_id) {
    if (lru_map_.count(frame_id)) {
        lru_list_.erase(lru_map_[frame_id]);
        lru_map_.erase(frame_id);
    }
    lru_list_.push_front(frame_id);
    lru_map_[frame_id] = lru_list_.begin();
}

bool LRUReplacer::Evict(uint32_t &out_frame_id) {
    if (lru_list_.empty()) return false;
    out_frame_id = lru_list_.back();
    lru_map_.erase(out_frame_id);
    lru_list_.pop_back();
    return true;
}

void LRUReplacer::Remove(uint32_t frame_id) {
    if (lru_map_.count(frame_id)) {
        lru_list_.erase(lru_map_[frame_id]);
        lru_map_.erase(frame_id);
    }
}

size_t LRUReplacer::Size() const { return lru_list_.size(); }

// ---- BufferPool ----

BufferPool::BufferPool(uint32_t pool_size, DiskManager &disk_manager)
    : pool_size_(pool_size),
      disk_manager_(disk_manager),
      frames_(pool_size),
      replacer_(pool_size) {
    for (uint32_t i = 0; i < pool_size; i++)
        free_list_.push_back(i);
}

BufferPool::~BufferPool() { FlushAllPages(); }

Page *BufferPool::FetchPage(uint32_t page_id) {
    if (page_table_.count(page_id)) {
        uint32_t frame_id = page_table_[page_id];
        frames_[frame_id].pin_count++;
        replacer_.Remove(frame_id);
        return &frames_[frame_id].page;
    }

    uint32_t frame_id;
    if (!GetFreeFrame(frame_id))
        throw std::runtime_error("FetchPage: no free frames");

    disk_manager_.ReadPage(page_id, frames_[frame_id].page.data);
    frames_[frame_id].page.SetPageId(page_id);  // FIX 1: set page ID for correct eviction
    frames_[frame_id].pin_count = 1;
    frames_[frame_id].is_dirty  = false;
    page_table_[page_id]        = frame_id;

    return &frames_[frame_id].page;
}

Page *BufferPool::NewPage(uint32_t &out_page_id) {
    uint32_t frame_id;
    if (!GetFreeFrame(frame_id))
        throw std::runtime_error("NewPage: no free frames");

    out_page_id = disk_manager_.AllocatePage();
    memset(frames_[frame_id].page.data, 0, PAGE_SIZE);
    frames_[frame_id].page.SetPageId(out_page_id);
    frames_[frame_id].pin_count = 1;
    frames_[frame_id].is_dirty  = true;
    page_table_[out_page_id]    = frame_id;

    return &frames_[frame_id].page;
}

void BufferPool::UnpinPage(uint32_t page_id, bool is_dirty) {
    if (!page_table_.count(page_id)) return;
    uint32_t frame_id = page_table_[page_id];
    Frame   &frame    = frames_[frame_id];

    // FIX 2: pin_count is uint32_t; use == 0 to avoid silent unsigned underflow
    if (frame.pin_count == 0)
        throw std::runtime_error("UnpinPage: pin count already 0");

    frame.pin_count--;
    if (is_dirty) frame.is_dirty = true;
    if (frame.pin_count == 0) replacer_.Insert(frame_id);
}

void BufferPool::FlushPage(uint32_t page_id) {
    if (!page_table_.count(page_id)) return;
    uint32_t frame_id = page_table_[page_id];

    // FIX 3: only write if dirty, consistent with FlushAllPages
    if (frames_[frame_id].is_dirty) {
        disk_manager_.WritePage(page_id, frames_[frame_id].page.data);
        frames_[frame_id].is_dirty = false;
    }
}

void BufferPool::FlushAllPages() {
    // FIX 5: replaced C++17 structured binding with C++14-compatible iterator
    for (auto it = page_table_.begin(); it != page_table_.end(); ++it) {
        uint32_t page_id  = it->first;
        uint32_t frame_id = it->second;
        if (frames_[frame_id].is_dirty) {
            disk_manager_.WritePage(page_id, frames_[frame_id].page.data);
            frames_[frame_id].is_dirty = false;
        }
    }
    disk_manager_.Flush();
}

bool BufferPool::GetFreeFrame(uint32_t &out_frame_id) {
    if (!free_list_.empty()) {
        out_frame_id = free_list_.front();
        free_list_.pop_front();
        return true;
    }
    if (!replacer_.Evict(out_frame_id)) return false;

    Frame   &frame           = frames_[out_frame_id];
    uint32_t evicted_page_id = frame.page.GetPageId();

    if (frame.is_dirty) {
        disk_manager_.WritePage(evicted_page_id, frame.page.data);
        frame.is_dirty = false;
    }

    page_table_.erase(evicted_page_id);
    frame.pin_count = 0;
    frame.page.SetPageId(INVALID_PAGE_ID);  // FIX 4: clear stale page ID after eviction
    return true;
}