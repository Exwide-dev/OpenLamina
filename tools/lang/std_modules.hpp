#pragma once

#include "irgen/opcode.hpp"

namespace lang {

irgen::ModuleObject make_iter_module();
irgen::ModuleObject make_io_module();
irgen::ModuleObject make_format_module();

} // namespace lang
