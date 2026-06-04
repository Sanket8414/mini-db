#include "execution/projection.h"
#include <stdexcept>

Projection::Projection(std::unique_ptr<Executor>       child,
                       const TableSchema              &schema,
                       const std::vector<std::string> &col_names,
                       bool                            select_all)
    : child_(std::move(child)),
      input_schema_(schema),
      select_all_(select_all) {

    if (select_all_) {
        // Keep all columns
        for (uint16_t i = 0; i < schema.columns.size(); i++) {
            col_indices_.push_back(i);
            output_schema_.columns.push_back(schema.columns[i]);
        }
    } else {
        // Keep only requested columns
        for (auto &name : col_names) {
            int idx = schema.GetColumnIndex(name);
            if (idx < 0)
                throw std::runtime_error("Projection: column not found: " + name);
            col_indices_.push_back(static_cast<uint16_t>(idx));
            output_schema_.columns.push_back(schema.columns[idx]);
        }
    }

    output_schema_.name = schema.name;
}

void Projection::Open() {
    child_->Open();
}

Tuple *Projection::Next() {
    Tuple *t = child_->Next();
    if (t == nullptr) return nullptr;

    std::vector<Value> projected;
    for (uint16_t idx : col_indices_) {
        projected.push_back(t->GetValue(idx));
    }

    current_tuple_ = Tuple(projected);
    return &current_tuple_;
}

void Projection::Close() {
    child_->Close();
}