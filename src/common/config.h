#pragma once

#include <cstdint>

static constexpr uint32_t PAGE_SIZE = 4096;
static constexpr uint32_t BUFFER_POOL_SIZE = 32;
static constexpr uint32_t INVALID_PAGE_ID = UINT32_MAX;
static constexpr uint16_t INVALID_SLOT_ID = UINT16_MAX;
static constexpr uint32_t MAX_VARCHAR_SIZE = 256;
static constexpr uint32_t CATALOG_PAGE_ID = 0;