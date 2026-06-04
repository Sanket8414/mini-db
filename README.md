# Mini Database Engine

A lightweight relational database engine built from scratch in C++17.

## Features (v1)

- **Storage Layer**: Page-based disk storage with slotted page layout
- **Buffer Pool**: LRU-based page caching in memory
- **B+ Tree Index**: Balanced tree index for fast lookups and range scans
- **SQL Support**: CREATE TABLE, INSERT, SELECT with WHERE predicates
- **Data Types**: INT, VARCHAR(n), NULL
- **CLI REPL**: Interactive command-line interface

## Build

```bash
mkdir build && cd build
cmake ..
make
```

Run:
```bash
./mini_db
```

Run tests:
```bash
make test
# or
ctest
```

## Project Structure

- `src/storage/` — disk manager, page layout, buffer pool
- `src/index/` — B+ Tree implementation
- `src/catalog/` — schema and table metadata
- `src/sql/` — lexer, parser, AST
- `src/execution/` — query execution engine
- `src/common/` — utility types and constants
- `test/` — unit and integration tests

## Example Usage

```sql
CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(50));
INSERT INTO users VALUES (1, 'Alice');
SELECT * FROM users WHERE id = 1;
```

## Architecture

See `docs/ARCHITECTURE.md` for a detailed overview.