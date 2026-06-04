#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <string>
#include "storage/buffer_pool.h"
#include "common/rid.h"
#include "common/config.h"

// ----------------------------------------------------------------
// On-disk node layout (each node = one 4096 byte page)
//
// Internal node:
//   [NodeHeader | key_0 | ptr_0 | key_1 | ptr_1 | ... | ptr_n]
//   n keys, n+1 child pointers (page_ids)
//
// Leaf node:
//   [NodeHeader | key_0 | rid_0 | key_1 | rid_1 | ... | next_ptr]
//   n keys, n RIDs, one next_leaf pointer at fixed position
// ----------------------------------------------------------------

// ---- Node header — sits at the front of every B+ Tree page ----
#pragma pack(push, 1)
struct BPNodeHeader {
    uint8_t  is_leaf;        // 1 = leaf node, 0 = internal node
    uint16_t num_keys;       // how many keys currently in this node
    uint32_t parent_page_id; // parent's page_id (INVALID if root)
};
#pragma pack(pop)

static constexpr uint16_t BP_NODE_HEADER_SIZE = sizeof(BPNodeHeader);

// ---- Capacity calculations ----
// Internal node: stores int32_t keys and uint32_t child pointers
// Layout: [header][key_0][ptr_0][key_1][ptr_1]...[ptr_n]
// Max keys in internal node:
static constexpr uint16_t INTERNAL_KEY_SIZE  = sizeof(int32_t);
static constexpr uint16_t INTERNAL_PTR_SIZE  = sizeof(uint32_t);
static constexpr uint16_t INTERNAL_MAX_KEYS  =
    (PAGE_SIZE - BP_NODE_HEADER_SIZE - INTERNAL_PTR_SIZE)
    / (INTERNAL_KEY_SIZE + INTERNAL_PTR_SIZE);

// Leaf node: stores int32_t keys and RID values
// Layout: [header][key_0][rid_0][key_1][rid_1]...[next_leaf_ptr]
static constexpr uint16_t LEAF_KEY_SIZE      = sizeof(int32_t);
static constexpr uint16_t LEAF_VAL_SIZE      = sizeof(RID);
static constexpr uint16_t LEAF_NEXT_PTR_SIZE = sizeof(uint32_t);
static constexpr uint16_t LEAF_MAX_KEYS      =
    (PAGE_SIZE - BP_NODE_HEADER_SIZE - LEAF_NEXT_PTR_SIZE)
    / (LEAF_KEY_SIZE + LEAF_VAL_SIZE);


// ----------------------------------------------------------------
// BPlusTree — integer key, RID value, all nodes stored on disk
// via the buffer pool
// ----------------------------------------------------------------
class BPlusTree {
public:
    // Create or load a B+ Tree
    // If root_page_id == INVALID_PAGE_ID, a new empty tree is created
    BPlusTree(BufferPool &bp, uint32_t root_page_id = INVALID_PAGE_ID);

    // Insert a key-RID pair into the tree
    // Returns false if key already exists
    bool Insert(int32_t key, const RID &rid);

    // Search for an exact key
    // Returns the RID if found, empty optional otherwise
    std::optional<RID> Search(int32_t key);

    // Range scan — return all RIDs with keys in [low, high] inclusive
    std::vector<RID> RangeScan(int32_t low, int32_t high);

    // Root page ID — needed by catalog to persist the tree
    uint32_t GetRootPageId() const { return root_page_id_; }

    // Is the tree empty?
    bool IsEmpty() const { return root_page_id_ == INVALID_PAGE_ID; }

private:
    BufferPool &bp_;
    uint32_t    root_page_id_;

    // ---- Node read helpers ----

    // Read the header of a node page
    BPNodeHeader ReadHeader(uint32_t page_id);

    // Read a key at index i from an internal or leaf node
    int32_t  ReadKey(Page *page, uint16_t index, bool is_leaf);

    // Read a child pointer at index i from an internal node
    uint32_t ReadChildPtr(Page *page, uint16_t index);

    // Read a RID at index i from a leaf node
    RID ReadRID(Page *page, uint16_t index);

    // Read the next-leaf pointer from a leaf node
    uint32_t ReadNextLeaf(Page *page);

    // ---- Node write helpers ----

    void WriteHeader(Page *page, const BPNodeHeader &header);
    void WriteKey(Page *page, uint16_t index, int32_t key, bool is_leaf);
    void WriteChildPtr(Page *page, uint16_t index, uint32_t ptr);
    void WriteRID(Page *page, uint16_t index, const RID &rid);
    void WriteNextLeaf(Page *page, uint32_t next_page_id);

    // ---- Byte offset calculations ----
    // These tell us exactly where in the page's raw bytes each field sits

    // Internal node: key at index i
    uint16_t InternalKeyOffset(uint16_t i);
    // Internal node: child pointer at index i
    uint16_t InternalPtrOffset(uint16_t i);
    // Leaf node: key at index i
    uint16_t LeafKeyOffset(uint16_t i);
    // Leaf node: RID at index i
    uint16_t LeafRIDOffset(uint16_t i);
    // Leaf node: next pointer (fixed at end)
    uint16_t LeafNextPtrOffset();

    // ---- Tree operations ----

    // Find the leaf page that should contain key
    uint32_t FindLeaf(int32_t key);

    // Insert into a leaf — returns true if the leaf overflowed
    bool InsertIntoLeaf(uint32_t leaf_page_id, int32_t key, const RID &rid);

    // Split a full leaf node
    // Creates a new leaf, redistributes keys, returns the new leaf's
    // first key (which gets pushed up to the parent)
    int32_t SplitLeaf(uint32_t leaf_page_id, uint32_t &new_leaf_page_id);

    // Insert a key + right child pointer into an internal node
    // Returns true if the internal node overflowed
    bool InsertIntoInternal(uint32_t internal_page_id,
                            int32_t  key,
                            uint32_t right_child_page_id);

    // Split a full internal node
    int32_t SplitInternal(uint32_t internal_page_id,
                          uint32_t &new_internal_page_id);

    // Push a key up to the parent after a split
    // Handles the case where the parent is also full (recursive splits)
    // Also handles creating a new root
    void PushUpKey(uint32_t left_page_id,
                   int32_t  key,
                   uint32_t right_page_id,
                   uint32_t parent_page_id);

    // Create a brand new root with two children
    void CreateNewRoot(uint32_t left_page_id,
                       int32_t  key,
                       uint32_t right_page_id);

    // Initialize a new empty leaf page
    void InitLeafPage(Page *page, uint32_t page_id,
                      uint32_t parent_page_id = INVALID_PAGE_ID);

    // Initialize a new empty internal page
    void InitInternalPage(Page *page, uint32_t page_id,
                          uint32_t parent_page_id = INVALID_PAGE_ID);
};