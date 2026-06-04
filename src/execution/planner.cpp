#include "execution/planner.h"
#include <stdexcept>
#include <cstring>

// windows.h (pulled in via disk_manager.h) defines GetFreeSpace as a Win32
// macro, which breaks the Page::GetFreeSpace() method call below. Undefine it.
#ifdef GetFreeSpace
#undef GetFreeSpace
#endif

Planner::Planner(Catalog &catalog, BufferPool &bp)
    : catalog_(catalog), bp_(bp) {}

std::unique_ptr<Executor> Planner::PlanSelect(SelectStmt &stmt) {
    TableSchema &schema = catalog_.GetTable(stmt.from_table);

    std::unique_ptr<Executor> scan;

    if (stmt.where_clause) {
        Value search_key;
        Value low_key, high_key;

        if (IsPKEquality(stmt.where_clause.get(), schema, search_key)) {
            scan = std::unique_ptr<Executor>(
                new IndexScan(stmt.from_table, catalog_, bp_, search_key));
        } else if (IsPKRange(stmt.where_clause.get(), schema,
                              low_key, high_key)) {
            scan = std::unique_ptr<Executor>(
                new IndexScan(stmt.from_table, catalog_, bp_, low_key, high_key));
        } else {
            std::unique_ptr<Executor> seq(
                new SeqScan(stmt.from_table, catalog_, bp_));
            scan = std::unique_ptr<Executor>(
                new Filter(std::move(seq), stmt.where_clause.get(), schema));
        }
    } else {
        scan = std::unique_ptr<Executor>(
            new SeqScan(stmt.from_table, catalog_, bp_));
    }

    bool select_all = stmt.IsSelectAll();
    return std::unique_ptr<Executor>(
        new Projection(std::move(scan), schema, stmt.columns, select_all));
}

void Planner::ExecuteInsert(InsertStmt &stmt) {
    TableSchema &schema = catalog_.GetTable(stmt.table_name);

    Tuple t(stmt.values);
    std::vector<char> serialized = t.Serialize();

    uint32_t page_id        = schema.first_page_id;
    Page    *target         = nullptr;
    uint32_t target_page_id = INVALID_PAGE_ID;

    if (page_id == INVALID_PAGE_ID) {
        target = bp_.NewPage(target_page_id);
        target->Init(target_page_id, PageType::HEAP);
        schema.first_page_id = target_page_id;
        catalog_.UpdateTable(schema);
    } else {
        // Walk pages to find one with enough space
        while (page_id != INVALID_PAGE_ID) {
            Page *page = bp_.FetchPage(page_id);
            if (page->GetFreeSpace() >= serialized.size() + SLOT_SIZE) {
                target         = page;
                target_page_id = page_id;
                break;
            }
            uint32_t next = page->GetNextPageId();
            bp_.UnpinPage(page_id, false);
            page_id = next;
        }

        if (target == nullptr) {
            // FIX (bug #3): walk to last page carefully, tracking last_page_id
            // and unpinning each page before moving on. Unpin last page AFTER
            // we have linked the new page in, but BEFORE calling NewPage so
            // the pool is never fully pinned.
            uint32_t last_page_id = INVALID_PAGE_ID;
            uint32_t cur          = schema.first_page_id;

            while (true) {
                Page    *p    = bp_.FetchPage(cur);
                uint32_t next = p->GetNextPageId();
                if (next == INVALID_PAGE_ID) {
                    last_page_id = cur;
                    // Unpin BEFORE NewPage to keep a free frame available
                    bp_.UnpinPage(cur, false);
                    break;
                }
                bp_.UnpinPage(cur, false);
                cur = next;
            }

            target = bp_.NewPage(target_page_id);
            target->Init(target_page_id, PageType::HEAP);

            // Re-fetch last page to set its next pointer
            Page *last_page = bp_.FetchPage(last_page_id);
            last_page->SetNextPageId(target_page_id);
            bp_.UnpinPage(last_page_id, true);
        }
    }

    uint16_t slot_id = target->InsertTuple(
        serialized.data(),
        static_cast<uint16_t>(serialized.size()));

    bp_.UnpinPage(target_page_id, true);

    // Insert into B+ Tree index if table has a primary key
    int pk_idx = schema.GetPrimaryKeyIndex();
    if (pk_idx >= 0) {
        Value pk_val = stmt.values[pk_idx];
        if (!pk_val.IsInt())
            throw std::runtime_error(
                "ExecuteInsert: only INT primary keys supported");

        BPlusTree tree(bp_, schema.root_page_id);
        RID rid(target_page_id, slot_id);
        bool inserted = tree.Insert(pk_val.GetInt(), rid);

        if (!inserted)
            throw std::runtime_error(
                "ExecuteInsert: duplicate primary key "
                + std::to_string(pk_val.GetInt()));

        if (schema.root_page_id != tree.GetRootPageId()) {
            schema.root_page_id = tree.GetRootPageId();
            catalog_.UpdateTable(schema);
        }
    }
}

void Planner::ExecuteCreate(CreateTableStmt &stmt) {
    TableSchema schema;
    schema.name = stmt.table_name;

    for (size_t i = 0; i < stmt.columns.size(); i++) {
        const ColumnDef &col_def = stmt.columns[i];
        ColumnSchema col;
        col.name           = col_def.name;
        col.type           = col_def.type;
        col.max_length     = col_def.max_length;
        col.is_primary_key = col_def.is_primary_key;
        schema.columns.push_back(col);
    }

    catalog_.CreateTable(schema);
    catalog_.Save();
}

// ---- Plan helpers ----

bool Planner::IsPKEquality(Expr *where_expr,
                            const TableSchema &schema,
                            Value &out_key) {
    if (!where_expr) return false;
    if (where_expr->type != ExprType::BINARY_EXPR) return false;

    BinaryExpr *bin = static_cast<BinaryExpr *>(where_expr);
    if (bin->op != BinaryOp::EQ) return false;

    int pk_idx = schema.GetPrimaryKeyIndex();
    if (pk_idx < 0) return false;

    if (bin->left->type  == ExprType::COLUMN_REF &&
        bin->right->type == ExprType::LITERAL) {
        ColumnRefExpr *col = static_cast<ColumnRefExpr *>(bin->left.get());
        if (col->col_index == pk_idx) {
            out_key = static_cast<LiteralExpr *>(bin->right.get())->value;
            return true;
        }
    }

    if (bin->right->type == ExprType::COLUMN_REF &&
        bin->left->type  == ExprType::LITERAL) {
        ColumnRefExpr *col = static_cast<ColumnRefExpr *>(bin->right.get());
        if (col->col_index == pk_idx) {
            out_key = static_cast<LiteralExpr *>(bin->left.get())->value;
            return true;
        }
    }

    return false;
}

bool Planner::IsPKRange(Expr *where_expr,
                         const TableSchema &schema,
                         Value &out_low, Value &out_high) {
    if (!where_expr) return false;
    if (where_expr->type != ExprType::BINARY_EXPR) return false;

    BinaryExpr *bin = static_cast<BinaryExpr *>(where_expr);
    if (bin->op != BinaryOp::AND) return false;

    int pk_idx = schema.GetPrimaryKeyIndex();
    if (pk_idx < 0) return false;

    Expr    *lhs      = bin->left.get();
    Expr    *rhs      = bin->right.get();
    Value    low, high;
    bool     got_low  = false;
    bool     got_high = false;

    if (lhs->type == ExprType::BINARY_EXPR) {
        BinaryExpr *b = static_cast<BinaryExpr *>(lhs);
        if (b->op == BinaryOp::GTE &&
            b->left->type  == ExprType::COLUMN_REF &&
            b->right->type == ExprType::LITERAL) {
            ColumnRefExpr *col = static_cast<ColumnRefExpr *>(b->left.get());
            if (col->col_index == pk_idx) {
                low     = static_cast<LiteralExpr *>(b->right.get())->value;
                got_low = true;
            }
        }
    }

    if (rhs->type == ExprType::BINARY_EXPR) {
        BinaryExpr *b = static_cast<BinaryExpr *>(rhs);
        if (b->op == BinaryOp::LTE &&
            b->left->type  == ExprType::COLUMN_REF &&
            b->right->type == ExprType::LITERAL) {
            ColumnRefExpr *col = static_cast<ColumnRefExpr *>(b->left.get());
            if (col->col_index == pk_idx) {
                high     = static_cast<LiteralExpr *>(b->right.get())->value;
                got_high = true;
            }
        }
    }

    if (got_low && got_high) {
        out_low  = low;
        out_high = high;
        return true;
    }

    return false;
}

Value Planner::ExtractLiteralValue(Expr *expr) {
    if (expr->type != ExprType::LITERAL)
        throw std::runtime_error("Planner: expected literal value");
    return static_cast<LiteralExpr *>(expr)->value;
}