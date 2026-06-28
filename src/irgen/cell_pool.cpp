#include "cell_pool.hpp"

#include "friend_function.hpp"
#include "opcode.hpp"

#include <new>
#include <unordered_map>
#include <vector>

namespace irgen {

namespace {

void resetCell(Value* cell) {
    cell->~Value();
    new (cell) Value();
}

struct CellDeleter {
    CellPool* pool = nullptr;

    void operator()(Value* cell) const {
        if (pool != nullptr) {
            pool->releaseCell(cell);
        } else {
            delete cell;
        }
    }
};

} // namespace

struct CellPool::Impl {
    struct CellRecord {
        uint32_t chunk = 0;
        uint32_t index = 0;
        uint8_t mark = 0;
        bool is_free = false;
    };

    // chunks 必须最后声明，以便析构时最先销毁；此时 cell_index 仍有效，
    // 避免 shared_ptr deleter 在 cell_index 已销毁后误对池内地址 delete。
    std::vector<Value*> free_list;
    std::unordered_map<Value*, CellRecord> cell_index;
    size_t allocations_since_gc = 0;
    std::vector<std::unique_ptr<Value[]>> chunks;

    static constexpr size_t kChunkSize = 4096;

    void recycle(Value* cell);
    [[nodiscard]] Value* acquireCell(CellPool& owner);
    void growChunk();
    [[nodiscard]] bool owns(const Value* cell) const;
    void clearMarks();
    void markCell(Value* cell);
    void markFromValue(const Value& value);
    void markModuleExports(const ModuleObject& module);
    void sweepUnmarked();
    void tryCollectBeforeGrow(CellPool& owner);
};

CellPool::CellPool() : impl_(std::make_unique<Impl>()) {
}

CellPool::CellPool(CellPool&&) noexcept = default;

CellPool& CellPool::operator=(CellPool&&) noexcept = default;

CellPool::~CellPool() {
    markDestroying();
}

void CellPool::bindOwner(VM* vm) {
    owner_vm_ = vm;
}

void CellPool::markDestroying() noexcept {
    destroying_ = true;
}

void CellPool::releaseCell(Value* cell) const {
    if (destroying_ || cell == nullptr) {
        return;
    }
    impl_->recycle(cell);
}

void CellPool::releaseAll() {
    impl_->free_list.clear();
    impl_->cell_index.clear();
    impl_->chunks.clear();
    impl_->allocations_since_gc = 0;
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
        cell_index[cell] = CellRecord{chunk_id, i, 0, true};
        free_list.push_back(cell);
    }
}

void CellPool::Impl::tryCollectBeforeGrow(CellPool& owner) {
    if (owner.owner_vm_ != nullptr && owner.owner_vm_->gc_suppress_depth == 0) {
        owner.collectGarbage(*owner.owner_vm_);
    }
}

Value* CellPool::Impl::acquireCell(CellPool& owner) {
    if (chunks.empty()) {
        growChunk();
    }
    if (free_list.empty()) {
        tryCollectBeforeGrow(owner);
    }
    if (free_list.empty()) {
        growChunk();
    }

    Value* cell = free_list.back();
    free_list.pop_back();
    cell_index[cell].mark = 0;
    cell_index[cell].is_free = false;
    return cell;
}

CellPool::CellPtr CellPool::allocate() {
    Value* cell = impl_->acquireCell(*this);
    resetCell(cell);
    ++impl_->allocations_since_gc;
    return CellPtr(cell, CellDeleter{this});
}

CellPool::CellPtr CellPool::allocateCopy(const Value& value) {
    Value* cell = impl_->acquireCell(*this);
    cell->~Value();
    new (cell) Value(value);
    ++impl_->allocations_since_gc;
    return CellPtr(cell, CellDeleter{this});
}

CellPool::CellPtr CellPool::allocateValue(Value&& value) {
    Value* cell = impl_->acquireCell(*this);
    cell->~Value();
    new (cell) Value(std::move(value));
    ++impl_->allocations_since_gc;
    return CellPtr(cell, CellDeleter{this});
}

void CellPool::Impl::recycle(Value* cell) {
    if (!owns(cell)) {
        // 非本池分配的堆槽位（理论上不应出现）；池内地址在 owns 失败时绝不能 delete。
        return;
    }

    auto& record = cell_index[cell];
    if (record.is_free) {
        return;
    }

    resetCell(cell);
    free_list.push_back(cell);
    record.is_free = true;
    record.mark = 0;
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
    if (value.getType() == Value::Type::FriendFunction) {
        const auto& ff = value.asFriendFunction();
        if (ff->dispatch_list_holder) {
            markCell(ff->dispatch_list_holder.get());
            for (const auto& handler : ff->dispatch_list_holder->asVector()) {
                if (handler) {
                    markFromValue(*handler);
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
    to_sweep.reserve(cell_index.size());

    for (auto& [cell, record] : cell_index) {
        if (record.mark == 0 && !record.is_free) {
            to_sweep.push_back(cell);
        }
    }

    free_list.reserve(free_list.size() + to_sweep.size());

    for (Value* cell : to_sweep) {
        resetCell(cell);
        free_list.push_back(cell);
        cell_index[cell].is_free = true;
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

} // namespace irgen
