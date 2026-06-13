#pragma once

#include <cstddef>
#include <memory>

namespace irgen {

class Value;
class VM;

/**
 * @brief 槽位对象池：每个 VM 持有一份；shared_ptr 自定义 deleter 回收到空闲链表；
 *        标记-清除 GC 只回收本 VM 根集合不可达的槽位。
 */
class CellPool {
public:
    using CellPtr = std::shared_ptr<Value>;

    CellPool();
    ~CellPool();

    CellPool(const CellPool&) = delete;
    CellPool& operator=(const CellPool&) = delete;
    CellPool(CellPool&&) noexcept;
    CellPool& operator=(CellPool&&) noexcept;

    void bindOwner(VM* vm);

    [[nodiscard]] CellPtr allocate();
    [[nodiscard]] CellPtr allocateCopy(const Value& value);
    [[nodiscard]] CellPtr allocateValue(Value&& value);
    void collectGarbage(const VM& vm) const;
    void releaseCell(Value* cell) const;

    [[nodiscard]] size_t totalCells() const;
    [[nodiscard]] size_t freeCells() const;
    [[nodiscard]] size_t liveCells() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    VM* owner_vm_ = nullptr;
};

} // namespace irgen
