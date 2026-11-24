#pragma once

#include "nodes.h"
#include "literals.h"
#include "types.h"
#include "declarations.h"
#include "header.h"
#include "name.h"
#include "operations.h"
#include "statements.h"

#include <string>

namespace xenon::ast {

    class AST {
        std::vector<StatementPtr> declarations_;
    public:
        std::string module_doc;

        AST() = default;

        void add_statement(StatementPtr statement) {
            declarations_.emplace_back(std::move(statement));
        }

        [[nodiscard]] const std::vector<StatementPtr>& declarations() const noexcept {
            return declarations_;
        }
    };

} // namespace xenon::ast
