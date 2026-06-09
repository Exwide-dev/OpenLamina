#include "cell_pool.hpp"

#include "opcode.hpp"

namespace irgen {

std::shared_ptr<Value> CellPool::allocate() {
    // 第 2 期：从空闲链表取槽；当前占位为直接分配
    return std::make_shared<Value>();
}

} // namespace irgen
