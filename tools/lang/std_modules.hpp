#pragma once

#include "irgen/opcode.hpp"

namespace lang {

irgen::ModuleObject make_iter_module();
irgen::ModuleObject make_io_module();
irgen::ModuleObject make_format_module();
irgen::ModuleObject make_typing_module();
irgen::ModuleObject make_random_module();

} // namespace lang
