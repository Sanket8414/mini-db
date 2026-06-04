#include "index/b_plus_tree.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>

BPlusTree::BPlusTree(BufferPool &bp, uint32_t root_page_id)
    : bp_(bp), root_page_id_(root_page_id) {}

// ============================================================
// Public API
// ============================================================

bool BPlusTree::Insert(int32_t key, const RID &rid) {
    if (IsEmpty()) {
        uint32_t page_id;
        Page *page = bp_.NewPage(page_id);
        InitLeafPage(page, page_id);
        root_page_id_ = page_id;
        bp_.UnpinPage(page_id, true);
    }

    uint32_t leaf_page_id = FindLeaf(key);

    // FIX (bug #4): check for duplicate without re-fetching inside InsertIntoLeaf
    // We fetch once, check, then insert in the same fetch to avoid double-pin.
    Page        *leaf = bp_.FetchPage(leaf_page_id);
    BPNodeHeader h;
    memcpy(&h, leaf->data, BP_NODE_HEADER_SIZE);

    for (uint16_t i = 0; i < h.num_keys; i++) {
        if (ReadKey(leaf, i, true) == key) {
            bp_.UnpinPage(leaf_page_id, false);
            return false; // duplicate
        }
    }

    // Find insertion position (keep keys sorted)
    uint16_t pos = 0;
    while (pos < h.num_keys && ReadKey(leaf, pos, true) < key) pos++;

    // Shift existing entries right
    for (uint16_t i = h.num_keys; i > pos; i--) {
        WriteKey(leaf, i, ReadKey(leaf, i - 1, true), true);
        WriteRID(leaf, i, ReadRID(leaf, i - 1));
    }

    WriteKey(leaf, pos, key, true);
    WriteRID(leaf, pos, rid);
    h.num_keys++;
    WriteHeader(leaf, h);

    bool overflowed = (h.num_keys >= LEAF_MAX_KEYS);
    bp_.UnpinPage(leaf_page_id, true);  // single unpin for the single fetch

    if (overflowed) {
        uint32_t new_leaf_page_id;
        int32_t  push_up_key = SplitLeaf(leaf_page_id, new_leaf_page_id);

        uint32_t parent_id = ReadHeader(leaf_page_id).parent_page_id;

        PushUpKey(leaf_page_id, push_up_key, new_leaf_page_id, parent_id);
    }

    return true;
}

std::optional<RID> BPlusTree::Search(int32_t key) {
    if (IsEmpty()) return std::nullopt;

    uint32_t     leaf_page_id = FindLeaf(key);
    Page        *leaf         = bp_.FetchPage(leaf_page_id);
    BPNodeHeader h            = ReadHeader(leaf_page_id);

    std::optional<RID> result;
    for (uint16_t i = 0; i < h.num_keys; i++) {
        if (ReadKey(leaf, i, true) == key) {
            result = ReadRID(leaf, i);
            break;
        }
    }

    bp_.UnpinPage(leaf_page_id, false);
    return result;
}

std::vector<RID> BPlusTree::RangeScan(int32_t low, int32_t high) {
    std::vector<RID> results;
    if (IsEmpty()) return results;

    uint32_t leaf_page_id = FindLeaf(low);

    while (leaf_page_id != INVALID_PAGE_ID) {
        Page        *leaf = bp_.FetchPage(leaf_page_id);
        BPNodeHeader h    = ReadHeader(leaf_page_id);

        bool done = false;
        for (uint16_t i = 0; i < h.num_keys; i++) {
            int32_t k = ReadKey(leaf, i, true);
            if (k > high) { done = true; break; }
            if (k >= low) results.push_back(ReadRID(leaf, i));
        }

        uint32_t next = ReadNextLeaf(leaf);
        bp_.UnpinPage(leaf_page_id, false);

        if (done) break;
        leaf_page_id = next;
    }

    return results;
}

// ============================================================
// Byte offset calculations
// ============================================================

uint16_t BPlusTree::InternalPtrOffset(uint16_t i) {
    return BP_NODE_HEADER_SIZE + i * (INTERNAL_PTR_SIZE + INTERNAL_KEY_SIZE);
}

uint16_t BPlusTree::InternalKeyOffset(uint16_t i) {
    return InternalPtrOffset(i) + INTERNAL_PTR_SIZE;
}

uint16_t BPlusTree::LeafKeyOffset(uint16_t i) {
    return BP_NODE_HEADER_SIZE + i * (LEAF_KEY_SIZE + LEAF_VAL_SIZE);
}

uint16_t BPlusTree::LeafRIDOffset(uint16_t i) {
    return LeafKeyOffset(i) + LEAF_KEY_SIZE;
}

uint16_t BPlusTree::LeafNextPtrOffset() {
    return BP_NODE_HEADER_SIZE +
           LEAF_MAX_KEYS * (LEAF_KEY_SIZE + LEAF_VAL_SIZE);
}

// ============================================================
// Node read helpers
// ============================================================

BPNodeHeader BPlusTree::ReadHeader(uint32_t page_id) {
    Page *page = bp_.FetchPage(page_id);
    BPNodeHeader h;
    memcpy(&h, page->data, BP_NODE_HEADER_SIZE);
    bp_.UnpinPage(page_id, false);
    return h;
}

int32_t BPlusTree::ReadKey(Page *page, uint16_t index, bool is_leaf) {
    uint16_t offset = is_leaf ? LeafKeyOffset(index) : InternalKeyOffset(index);
    int32_t key;
    memcpy(&key, page->data + offset, sizeof(int32_t));
    return key;
}

uint32_t BPlusTree::ReadChildPtr(Page *page, uint16_t index) {
    uint32_t ptr;
    memcpy(&ptr, page->data + InternalPtrOffset(index), sizeof(uint32_t));
    return ptr;
}

RID BPlusTree::ReadRID(Page *page, uint16_t index) {
    RID rid;
    memcpy(&rid, page->data + LeafRIDOffset(index), sizeof(RID));
    return rid;
}

uint32_t BPlusTree::ReadNextLeaf(Page *page) {
    uint32_t next;
    memcpy(&next, page->data + LeafNextPtrOffset(), sizeof(uint32_t));
    return next;
}

// ============================================================
// Node write helpers
// ============================================================

void BPlusTree::WriteHeader(Page *page, const BPNodeHeader &header) {
    memcpy(page->data, &header, BP_NODE_HEADER_SIZE);
}

void BPlusTree::WriteKey(Page *page, uint16_t index, int32_t key, bool is_leaf) {
    uint16_t offset = is_leaf ? LeafKeyOffset(index) : InternalKeyOffset(index);
    memcpy(page->data + offset, &key, sizeof(int32_t));
}

void BPlusTree::WriteChildPtr(Page *page, uint16_t index, uint32_t ptr) {
    memcpy(page->data + InternalPtrOffset(index), &ptr, sizeof(uint32_t));
}

void BPlusTree::WriteRID(Page *page, uint16_t index, const RID &rid) {
    memcpy(page->data + LeafRIDOffset(index), &rid, sizeof(RID));
}

void BPlusTree::WriteNextLeaf(Page *page, uint32_t next_page_id) {
    memcpy(page->data + LeafNextPtrOffset(), &next_page_id, sizeof(uint32_t));
}

// ============================================================
// Node initialization
// ============================================================

void BPlusTree::InitLeafPage(Page *page, uint32_t page_id,
                              uint32_t parent_page_id) {
    memset(page->data, 0, PAGE_SIZE);
    BPNodeHeader h;
    h.is_leaf        = 1;
    h.num_keys       = 0;
    h.parent_page_id = parent_page_id;
    WriteHeader(page, h);
    WriteNextLeaf(page, INVALID_PAGE_ID);
}

void BPlusTree::InitInternalPage(Page *page, uint32_t page_id,
                                  uint32_t parent_page_id) {
    memset(page->data, 0, PAGE_SIZE);
    BPNodeHeader h;
    h.is_leaf        = 0;
    h.num_keys       = 0;
    h.parent_page_id = parent_page_id;
    WriteHeader(page, h);
}

// ============================================================
// FindLeaf
// ============================================================

uint32_t BPlusTree::FindLeaf(int32_t key) {
    uint32_t cur = root_page_id_;

    while (true) {
        Page        *page = bp_.FetchPage(cur);
        BPNodeHeader h;
        memcpy(&h, page->data, BP_NODE_HEADER_SIZE);

        if (h.is_leaf) {
            bp_.UnpinPage(cur, false);
            return cur;
        }

        uint32_t next  = INVALID_PAGE_ID;
        bool     found = false;
        for (uint16_t i = 0; i < h.num_keys; i++) {
            if (key < ReadKey(page, i, false)) {
                next  = ReadChildPtr(page, i);
                found = true;
                break;
            }
        }
        if (!found) next = ReadChildPtr(page, h.num_keys);

        bp_.UnpinPage(cur, false);
        cur = next;
    }
}

// ============================================================
// SplitLeaf
// ============================================================

int32_t BPlusTree::SplitLeaf(uint32_t leaf_page_id,
                              uint32_t &new_leaf_page_id) {
    Page        *old_leaf = bp_.FetchPage(leaf_page_id);
    BPNodeHeader old_h;
    memcpy(&old_h, old_leaf->data, BP_NODE_HEADER_SIZE);

    Page *new_leaf = bp_.NewPage(new_leaf_page_id);
    InitLeafPage(new_leaf, new_leaf_page_id, old_h.parent_page_id);

    uint16_t split = old_h.num_keys / 2;
    uint16_t new_n = old_h.num_keys - split;

    BPNodeHeader new_h;
    memcpy(&new_h, new_leaf->data, BP_NODE_HEADER_SIZE);
    for (uint16_t i = 0; i < new_n; i++) {
        WriteKey(new_leaf, i, ReadKey(old_leaf, split + i, true), true);
        WriteRID(new_leaf, i, ReadRID(old_leaf, split + i));
    }
    new_h.num_keys = new_n;
    WriteHeader(new_leaf, new_h);

    uint32_t old_next = ReadNextLeaf(old_leaf);
    WriteNextLeaf(new_leaf, old_next);
    WriteNextLeaf(old_leaf, new_leaf_page_id);

    old_h.num_keys = split;
    WriteHeader(old_leaf, old_h);

    int32_t push_up_key = ReadKey(new_leaf, 0, true);

    bp_.UnpinPage(leaf_page_id,     true);
    bp_.UnpinPage(new_leaf_page_id, true);

    return push_up_key;
}

// ============================================================
// InsertIntoInternal
// ============================================================

bool BPlusTree::InsertIntoInternal(uint32_t internal_page_id,
                                   int32_t  key,
                                   uint32_t right_child_page_id) {
    Page        *node = bp_.FetchPage(internal_page_id);
    BPNodeHeader h;
    memcpy(&h, node->data, BP_NODE_HEADER_SIZE);

    uint16_t pos = 0;
    while (pos < h.num_keys && ReadKey(node, pos, false) < key) pos++;

    for (uint16_t i = h.num_keys; i > pos; i--) {
        WriteKey(node, i, ReadKey(node, i - 1, false), false);
        WriteChildPtr(node, i + 1, ReadChildPtr(node, i));
    }

    WriteKey(node, pos, key, false);
    WriteChildPtr(node, pos + 1, right_child_page_id);
    h.num_keys++;
    WriteHeader(node, h);

    bool overflowed = (h.num_keys >= INTERNAL_MAX_KEYS);
    bp_.UnpinPage(internal_page_id, true);
    return overflowed;
}

// ============================================================
// SplitInternal
// ============================================================

int32_t BPlusTree::SplitInternal(uint32_t internal_page_id,
                                  uint32_t &new_internal_page_id) {
    Page        *old_node = bp_.FetchPage(internal_page_id);
    BPNodeHeader old_h;
    memcpy(&old_h, old_node->data, BP_NODE_HEADER_SIZE);

    Page *new_node = bp_.NewPage(new_internal_page_id);
    InitInternalPage(new_node, new_internal_page_id, old_h.parent_page_id);

    uint16_t mid     = old_h.num_keys / 2;
    int32_t  mid_key = ReadKey(old_node, mid, false);

    BPNodeHeader new_h;
    memcpy(&new_h, new_node->data, BP_NODE_HEADER_SIZE);
    uint16_t new_n = old_h.num_keys - mid - 1;

    WriteChildPtr(new_node, 0, ReadChildPtr(old_node, mid + 1));
    for (uint16_t i = 0; i < new_n; i++) {
        WriteKey(new_node, i, ReadKey(old_node, mid + 1 + i, false), false);
        WriteChildPtr(new_node, i + 1, ReadChildPtr(old_node, mid + 2 + i));
    }
    new_h.num_keys = new_n;
    WriteHeader(new_node, new_h);

    for (uint16_t i = 0; i <= new_n; i++) {
        uint32_t child_id = ReadChildPtr(new_node, i);
        if (child_id != INVALID_PAGE_ID) {
            Page        *child = bp_.FetchPage(child_id);
            BPNodeHeader ch;
            memcpy(&ch, child->data, BP_NODE_HEADER_SIZE);
            ch.parent_page_id = new_internal_page_id;
            WriteHeader(child, ch);
            bp_.UnpinPage(child_id, true);
        }
    }

    old_h.num_keys = mid;
    WriteHeader(old_node, old_h);

    bp_.UnpinPage(internal_page_id,     true);
    bp_.UnpinPage(new_internal_page_id, true);

    return mid_key;
}

// ============================================================
// PushUpKey
// ============================================================

void BPlusTree::PushUpKey(uint32_t left_page_id,
                           int32_t  key,
                           uint32_t right_page_id,
                           uint32_t parent_page_id) {
    if (parent_page_id == INVALID_PAGE_ID) {
        CreateNewRoot(left_page_id, key, right_page_id);
        return;
    }

    bool overflowed = InsertIntoInternal(parent_page_id, key, right_page_id);

    if (overflowed) {
        uint32_t new_internal_page_id;
        int32_t  push_up_key = SplitInternal(parent_page_id,
                                              new_internal_page_id);

        Page        *parent = bp_.FetchPage(parent_page_id);
        BPNodeHeader ph;
        memcpy(&ph, parent->data, BP_NODE_HEADER_SIZE);
        uint32_t grandparent_id = ph.parent_page_id;
        bp_.UnpinPage(parent_page_id, false);

        PushUpKey(parent_page_id, push_up_key,
                  new_internal_page_id, grandparent_id);
    }
}

// ============================================================
// CreateNewRoot
// ============================================================

void BPlusTree::CreateNewRoot(uint32_t left_page_id,
                               int32_t  key,
                               uint32_t right_page_id) {
    uint32_t new_root_page_id;
    Page    *new_root = bp_.NewPage(new_root_page_id);
    InitInternalPage(new_root, new_root_page_id);

    BPNodeHeader h;
    memcpy(&h, new_root->data, BP_NODE_HEADER_SIZE);
    h.num_keys = 1;
    WriteHeader(new_root, h);
    WriteChildPtr(new_root, 0, left_page_id);
    WriteKey(new_root, 0, key, false);
    WriteChildPtr(new_root, 1, right_page_id);
    bp_.UnpinPage(new_root_page_id, true);

    Page *left = bp_.FetchPage(left_page_id);
    BPNodeHeader lh;
    memcpy(&lh, left->data, BP_NODE_HEADER_SIZE);
    lh.parent_page_id = new_root_page_id;
    WriteHeader(left, lh);
    bp_.UnpinPage(left_page_id, true);

    Page *right = bp_.FetchPage(right_page_id);
    BPNodeHeader rh;
    memcpy(&rh, right->data, BP_NODE_HEADER_SIZE);
    rh.parent_page_id = new_root_page_id;
    WriteHeader(right, rh);
    bp_.UnpinPage(right_page_id, true);

    root_page_id_ = new_root_page_id;
}