#pragma once

#include "common/dataclasses.h"

namespace xenon {
    namespace driver {
        class ModuleNamespaceTree;
    }
}

namespace xenon::semantic {

    class SemanticAnalyser {
    public:
        static bool validate(const driver::ModuleNamespaceTree& namespace_tree,
            const config::CompilerConfig& options)
        {
            (void)namespace_tree;
            (void)options;
            return true;
        }
    };

} // namespace xenon::semantic
