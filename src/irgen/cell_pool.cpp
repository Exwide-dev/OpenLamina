#include <algorithm>
#include <new>
#include <unordered_map>
#include <vector>
#include "cell_pool.hpp"
#include "opcode.hpp"

namespace irgen {

namespace {

void resetCell(Value* cell) {
    cell->~Value();
    new (cell) Value();
}

struct CellDeleter {
    void operator()(Value* cell) const;
};

} // namespace

struct CellPool::Impl {
    struct CellRecord {
        uint32_t chunk = 0;
        uint32_t index = 0;
        uint8_t mark = 0;
    };

    std::vector<std::unique_ptr<Value[]>> chunks;
    std::vector<Value*> free_list;
    std::unordered_map<Value*, CellRecord> cell_index;

    VM* active_vm = nullptr;
    size_t allocations_since_gc = 0;

    static constexpr size_t kChunkSize = 4096;

    void recycle(Value* cell);
    [[nodiscard]] Value* acquireCell();
    void growChunk();
    [[nodiscard]] bool owns(const Value* cell) const;
    void clearMarks();
    void markCell(Value* cell);
    void markFromValue(const Value& value);
    void markModuleExports(const ModuleObject& module);
    void sweepUnmarked();
    void tryCollectBeforeGrow() const;
};

void CellDeleter::operator()(Value* cell) const {
    CellPool::instance().releaseCell(cell);
}

CellPool& CellPool::instance() {
    static CellPool pool;
    return pool;
}

CellPool::CellPool() : impl_(std::make_unique<Impl>()) {
}

CellPool::~CellPool() = default;

void CellPool::releaseCell(Value* cell) const {
    impl_->recycle(cell);
}

size_t CellPool::totalCells() const {
    return impl_->cell_index.size();
}

size_t CellPool::freeCells() const {
    return impl_->free_list.size();
}

size_t CellPool::liveCells() const {
    return impl_->cell_index.size() - impl_->free_list.size();
}

void CellPool::setActiveVm(VM* vm) const {
    impl_->active_vm = vm;
}

bool CellPool::Impl::owns(const Value* cell) const {
    return cell != nullptr && cell_index.contains(const_cast<Value*>(cell));
}

void CellPool::Impl::growChunk() {
    const auto chunk_id = static_cast<uint32_t>(chunks.size());
    auto chunk = std::make_unique<Value[]>(kChunkSize);
    Value* base = chunk.get();
    chunks.push_back(std::move(chunk));

    for (uint32_t i = 0; i < kChunkSize; ++i) {
        Value* cell = base + i;
        cell_index[cell] = CellRecord{chunk_id, i, 0};
        free_list.push_back(cell);
    }
}

Value* CellPool::Impl::acquireCell() {
    if (chunks.empty()) {
        growChunk();
    }
    if (free_list.empty()) {
        tryCollectBeforeGrow();
    }
    if (free_list.empty()) {
        growChunk();
    }

    Value* cell = free_list.back();
    free_list.pop_back();
    cell_index[cell].mark = 0;
    return cell;
}

void CellPool::Impl::tryCollectBeforeGrow() const {
    if (active_vm != nullptr) {
        instance().collectGarbage(*active_vm);
    }
}

CellPool::CellPtr CellPool::allocate() const {
    Value* cell = impl_->acquireCell();
    resetCell(cell);
    ++impl_->allocations_since_gc;
    return CellPtr(cell, CellDeleter{});
}

CellPool::CellPtr CellPool::allocateCopy(const Value& value) const {
    Value* cell = impl_->acquireCell();
    cell->~Value();
    new (cell) Value(value);
    ++impl_->allocations_since_gc;
    return CellPtr(cell, CellDeleter{});
}

CellPool::CellPtr CellPool::allocateValue(Value&& value) const {
    Value* cell = impl_->acquireCell();
    cell->~Value();
    new (cell) Value(std::move(value));
    ++impl_->allocations_since_gc;
    return CellPtr(cell, CellDeleter{});
}

void CellPool::Impl::recycle(Value* cell) {
    if (!owns(cell)) {
        delete cell;
        return;
    }

    resetCell(cell);
    if (std::ranges::find(free_list, cell) == free_list.end()) {
        free_list.push_back(cell);
    }
    cell_index[cell].mark = 0;
}

void CellPool::Impl::clearMarks() {
    for (auto& [cell, record] : cell_index) {
        (void)cell;
        record.mark = 0;
    }
}

void CellPool::Impl::markCell(Value* cell) {
    if (cell == nullptr || !owns(cell)) {
        return;
    }

    auto& record = cell_index[cell];
    if (record.mark != 0) {
        return;
    }
    record.mark = 1;
    markFromValue(*cell);
}

void CellPool::Impl::markFromValue(const Value& value) {
    if (value.isReference()) {
        const Ref& ref = value.asReference();
        if (ref.value_ptr) {
            markCell(ref.value_ptr.get());
        }
        return;
    }
    if (value.isVector()) {
        for (const auto& elem : value.asVector()) {
            if (elem) {
                markCell(elem.get());
            }
        }
        return;
    }
    if (value.isDictionary()) {
        for (const auto& [key, val] : value.asDictionary()) {
            if (key) {
                markCell(key.get());
            }
            if (val) {
                markCell(val.get());
            }
        }
        return;
    }
    if (value.isStruct()) {
        if (const auto& obj = value.asStruct()) {
            for (const Value& slot : obj->slots) {
                markFromValue(slot);
            }
        }
        return;
    }
    if (value.isUserFunction()) {
        if (const auto& func = value.asFunctionObject()) {
            for (const SymbolTable& table : func->closure) {
                for (const auto& [id, ptr] : table.symbols) {
                    (void)id;
                    if (ptr) {
                        markCell(ptr.get());
                    }
                }
            }
        }
        return;
    }
    if (value.getType() == Value::Type::Module) {
        if (const auto mod = value.asModule()) {
            markModuleExports(*mod);
        }
        return;
    }
    if (value.getType() == Value::Type::Iterator) {
        const auto iter = value.asIterator();
        if (iter && iter->iterable) {
            markCell(iter->iterable.get());
        }
    }
}

void CellPool::Impl::markModuleExports(const ModuleObject& module) {
    for (const auto& [name, val] : module.exports) {
        (void)name;
        markFromValue(val);
    }
    for (const auto& [name, sub] : module.submodules) {
        (void)name;
        if (sub) {
            markModuleExports(*sub);
        }
    }
}

[[gnu::hot]] void CellPool::Impl::sweepUnmarked() {
    std::vector<Value*> to_sweep;
    to_sweep.reserve(free_list.size());

    for (auto& [cell, record] : cell_index) {
        if (record.mark == 0 &&
            std::ranges::find(free_list, cell) == free_list.end()) {
            to_sweep.push_back(cell);
        }
    }

    for (Value* cell : to_sweep) {
        resetCell(cell);
        free_list.push_back(cell);
        cell_index[cell].mark = 0;
    }
}

void CellPool::collectGarbage(const VM& vm) const {
    impl_->clearMarks();

    for (const SymbolTable& table : vm.symbol_stack) {
        for (const auto& [id, ptr] : table.symbols) {
            (void)id;
            if (ptr) {
                impl_->markCell(ptr.get());
            }
        }
    }

    for (const Value& val : vm.op_stack.items()) {
        impl_->markFromValue(val);
    }

    for (const std::vector<Value>& frame : vm.locals_stack) {
        for (const Value& val : frame) {
            impl_->markFromValue(val);
        }
    }

    for (const std::shared_ptr<FunctionObject>& func : vm.call_func_stack) {
        if (!func) {
            continue;
        }
        for (const SymbolTable& table : func->closure) {
            for (const auto& [id, ptr] : table.symbols) {
                (void)id;
                if (ptr) {
                    impl_->markCell(ptr.get());
                }
            }
        }
    }

    for (const auto& [id, ptr] : vm.cache.allEntries()) {
        (void)id;
        if (ptr) {
            impl_->markCell(ptr.get());
        }
    }

    if (vm.main_module) {
        impl_->markModuleExports(*vm.main_module);
    }

    impl_->sweepUnmarked();
    impl_->allocations_since_gc = 0;
}

std::shared_ptr<Value> makePooledCell() {
    return CellPool::instance().allocate();
}

std::shared_ptr<Value> makePooledCell(const Value& value) {
    return CellPool::instance().allocateCopy(value);
}

std::shared_ptr<Value> makePooledCell(Value&& value) {
    return CellPool::instance().allocateValue(std::move(value));
}

} // namespace irgen
