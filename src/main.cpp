#include <iostream>
#include <string>
#include <memory>
#include <algorithm>

#include "storage/disk_manager.h"
#include "storage/buffer_pool.h"
#include "catalog/catalog.h"
#include "sql/lexer.h"
#include "sql/parser.h"
#include "execution/binder.h"
#include "execution/planner.h"
#include "execution/projection.h"

// Print a row of results
static void PrintTuple(const Tuple &t, const TableSchema &schema) {
    for (size_t i = 0; i < schema.columns.size(); i++) {
        if (i > 0) std::cout << " | ";
        std::cout << t.GetValue(static_cast<uint16_t>(i)).ToString();
    }
    std::cout << "\n";
}

// Print column headers
static void PrintHeader(const TableSchema &schema) {
    for (size_t i = 0; i < schema.columns.size(); i++) {
        if (i > 0) std::cout << " | ";
        std::cout << schema.columns[i].name;
    }
    std::cout << "\n";

    for (size_t i = 0; i < schema.columns.size(); i++) {
        if (i > 0) std::cout << "-+-";
        std::cout << std::string(schema.columns[i].name.size(), '-');
    }
    std::cout << "\n";
}

// Execute one SQL statement
static void ExecuteSQL(const std::string &sql,
                       Catalog           &catalog,
                       BufferPool        &bp) {
    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.Tokenize();
        Parser parser(tokens);
        auto   stmt = parser.ParseStatement();

        if (!stmt) return;

        Binder  binder(catalog);
        Planner planner(catalog, bp);

        if (stmt->type == StmtType::CREATE_TABLE) {
            auto *create = static_cast<CreateTableStmt *>(stmt.get());
            binder.BindCreate(*create);
            planner.ExecuteCreate(*create);
            std::cout << "Table '" << create->table_name
                      << "' created.\n";

        } else if (stmt->type == StmtType::INSERT) {
            auto *insert = static_cast<InsertStmt *>(stmt.get());
            binder.BindInsert(*insert);
            planner.ExecuteInsert(*insert);
            std::cout << "1 row inserted.\n";

        } else if (stmt->type == StmtType::SELECT) {
            auto *select = static_cast<SelectStmt *>(stmt.get());
            binder.BindSelect(*select);

            auto executor = planner.PlanSelect(*select);
            executor->Open();

            // Get output schema for printing headers
            auto *proj = dynamic_cast<Projection *>(executor.get());
            const TableSchema &out_schema =
                proj ? proj->GetOutputSchema()
                     : catalog.GetTable(select->from_table);

            PrintHeader(out_schema);

            int    row_count = 0;
            Tuple *t;
            while ((t = executor->Next()) != nullptr) {
                PrintTuple(*t, out_schema);
                row_count++;
            }

            executor->Close();
            std::cout << "(" << row_count << " row"
                      << (row_count == 1 ? "" : "s") << ")\n";
        }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

int main() {
    std::string db_file;
    std::unique_ptr<DiskManager> disk_manager;
    std::unique_ptr<BufferPool>  buffer_pool;
    std::unique_ptr<Catalog>     catalog;

    bool db_open = false;

    std::cout << "mini-db v1.0  |  type .help for commands\n";

    std::string line;
    std::string sql_buffer; // accumulates multi-line SQL

    while (true) {
        std::cout << (db_open ? "mini-db> " : "(no db)> ");
        if (!std::getline(std::cin, line)) break;

        // Trim whitespace
        auto trim = [](std::string &s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end   = s.find_last_not_of(" \t\r\n");
            s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };
        trim(line);

        if (line.empty()) continue;

        // --- Dot commands ---
        if (line[0] == '.') {
            if (line == ".exit" || line == ".quit") {
                if (db_open) {
                    buffer_pool->FlushAllPages();
                    std::cout << "Database saved. Goodbye.\n";
                }
                break;

            } else if (line.substr(0, 5) == ".open") {
                db_file = line.substr(6);
                trim(db_file);
                if (db_file.empty()) {
                    std::cerr << "Usage: .open <filename>\n";
                    continue;
                }
                try {
                    disk_manager = std::make_unique<DiskManager>(db_file);

                    // Ensure catalog page exists
                    if (disk_manager->GetNumPages() == 0) {
                        disk_manager->AllocatePage(); // page 0 = catalog
                    }

                    buffer_pool = std::make_unique<BufferPool>(
                        BUFFER_POOL_SIZE, *disk_manager);
                    catalog     = std::make_unique<Catalog>(*buffer_pool);
                    catalog->Load();
                    db_open = true;
                    std::cout << "Opened database: " << db_file << "\n";
                } catch (const std::exception &e) {
                    std::cerr << "Error opening database: " << e.what() << "\n";
                }

            } else if (line == ".tables") {
                if (!db_open) { std::cerr << "No database open.\n"; continue; }
                auto tables = catalog->ListTables();
                if (tables.empty()) {
                    std::cout << "(no tables)\n";
                } else {
                    for (auto &t : tables) std::cout << t << "\n";
                }

            } else if (line.substr(0, 7) == ".schema") {
                if (!db_open) { std::cerr << "No database open.\n"; continue; }
                std::string tname = line.substr(7);
                trim(tname);
                if (tname.empty()) {
                    std::cerr << "Usage: .schema <table>\n";
                    continue;
                }
                try {
                    TableSchema &s = catalog->GetTable(tname);
                    std::cout << "Table: " << s.name << "\n";
                    for (auto &col : s.columns) {
                        std::cout << "  " << col.name << "  "
                                  << TypeIdToString(col.type);
                        if (col.type == TypeId::VARCHAR)
                            std::cout << "(" << col.max_length << ")";
                        if (col.is_primary_key)
                            std::cout << "  PRIMARY KEY";
                        std::cout << "\n";
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Error: " << e.what() << "\n";
                }

            } else if (line == ".help") {
                std::cout
                    << "Commands:\n"
                    << "  .open <file>     Open or create a database file\n"
                    << "  .tables          List all tables\n"
                    << "  .schema <table>  Show table schema\n"
                    << "  .exit            Save and exit\n"
                    << "  .help            Show this help\n"
                    << "SQL: CREATE TABLE, INSERT INTO, SELECT ... FROM ... WHERE\n";

            } else {
                std::cerr << "Unknown command: " << line
                          << "  (try .help)\n";
            }

            continue;
        }

        // --- SQL input ---
        if (!db_open) {
            std::cerr << "No database open. Use .open <filename> first.\n";
            continue;
        }

        // Accumulate lines until we see a semicolon
        sql_buffer += line + " ";
        if (sql_buffer.find(';') != std::string::npos) {
            ExecuteSQL(sql_buffer, *catalog, *buffer_pool);
            sql_buffer.clear();
        }
    }

    return 0;
}