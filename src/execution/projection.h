#pragma once

#include "execution/executor.h"
#include "catalog/schema.h"
#include <vector>
#include <string>
#include <memory>

// Projection — picks only the requested columns from each tuple
// Handles SELECT * (all columns) and SELECT col1, col2
class Projection : public Executor {
public:
    // select_all = true means SELECT *
    Projection(std::unique_ptr<Executor>    child,
               const TableSchema           &schema,
               const std::vector<std::string> &col_names,
               bool                         select_all);

    void   Open()  override;
    Tuple *Next()  override;
    void   Close() override;

    // Schema of the output tuples (only selected columns)
    const TableSchema &GetOutputSchema() const { return output_schema_; }

private:
    std::unique_ptr<Executor> child_;
    TableSchema               input_schema_;
    TableSchema               output_schema_;
    std::vector<uint16_t>     col_indices_; // which columns to keep
    bool                      select_all_;
};