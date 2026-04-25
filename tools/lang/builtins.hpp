#pragma once
#include <vector>

namespace irgen {
    class Value;
    class VM;
    class SymbolTable;
}

namespace lang {
    using irgen::Value, irgen::VM;
    using ArgT = const std::vector<Value>&;

    Value input(VM&, ArgT);
    Value print(VM&, ArgT);
    Value copyright(VM&, ArgT);
    Value help(VM&, ArgT);
    Value exit(VM&, ArgT);

    void init_builtins(irgen::SymbolTable& symbols);
}
