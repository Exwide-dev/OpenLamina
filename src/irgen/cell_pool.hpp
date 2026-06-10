#pragma once

#include <memory>

namespace irgen {

class Value;
class VM;

/**
 * @brief 槽位对象池：预分配 Value 单元，shared_ptr 自定义 deleter 回收到空闲链表；
 *        标记-清除 GC 回收循环引用等 refcount 无法释放的单元。
 */
class CellPool {
public:
    using CellPtr = std::shared_ptr<Value>;

    static CellPool& instance();

    [[nodiscard]] CellPtr allocate() const;
    [[nodiscard]] CellPtr allocateCopy(const Value& value) const;
    CellPtr allocateValue(Value&& value) const;
    void collectGarbage(const VM& vm) const;
    void setActiveVm(VM* vm) const;

    [[nodiscard]] size_t totalCells() const;
    [[nodiscard]] size_t freeCells() const;
    [[nodiscard]] size_t liveCells() const;
    void releaseCell(Value* cell) const;

    ~CellPool();
    CellPool(const CellPool&) = delete;
    CellPool& operator=(const CellPool&) = delete;

private:
    CellPool();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace irgen
