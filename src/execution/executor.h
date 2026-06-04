#pragma once

#include <memory>
#include "execution/tuple.h"
#include "catalog/schema.h"

// Base class for all execution operators
// Uses the Volcano/iterator model:
//   Open()  — initialize the operator
//   Next()  — return one tuple at a time, nullptr when done
//   Close() — clean up
class Executor {
public:
    virtual ~Executor() = default;

    virtual void   Open()  = 0;
    virtual Tuple *Next()  = 0;
    virtual void   Close() = 0;

protected:
    Tuple current_tuple_; // reused buffer to avoid allocations
};