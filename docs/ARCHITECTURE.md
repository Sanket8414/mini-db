# Architecture Overview

## Layers

### 1. Storage Layer
Manages reading and writing fixed-size pages to disk.
- **DiskManager**: Raw file I/O using pread/pwrite
- **Page**: Slotted page format with variable-length tuples
- **BufferPool**: LRU cache of pages in memory

### 2. Index Layer
B+ Tree for primary key indexing.
- **BPlusTree**: Search, insert, range scan operations
- On-disk node storage (each node = one page)
- Balanced splits and merges

### 3. Catalog
Table and column metadata.
- **Schema**: Describes table structure
- **Catalog**: Stores and persists all schemas

### 4. SQL Frontend
Parsing SQL into an AST.
- **Lexer**: Tokenize SQL strings
- **Parser**: Recursive descent parser
- **AST**: Abstract syntax tree representation

### 5. Execution Engine
Query planning and execution.
- **Binder**: Resolve column names against catalog
- **Planner**: Create physical execution plan
- **Executors**: SeqScan, IndexScan, Filter, Projection
- **Volcano Model**: Iterator-based execution

## Data Flow