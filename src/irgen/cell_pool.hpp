#pragma once

#include <memory>

namespace irgen {

class Value;

/**
 * @brief 槽位对象池（第 2 期：统一从池分配 Value 槽位，shared_ptr 归零回池）
 *
 * 当前为占位实现，仍使用 std::make_shared；后续在此接入空闲链表与标记-清除 GC。
 */
class CellPool {
public:
    static CellPool& instance() {
        static CellPool pool;
        return pool;
    }

    std::shared_ptr<Value> allocate();
};

} // namespace irgen
