#include "iterator_ops.hpp"

#include "../tools/lang/type_methods.hpp"
#include "exceptions.hpp"
#include "struct_types.hpp"

#include <cstring>
#include <optional>
#include <ranges>
#include <utility>

namespace irgen {
namespace {

[[nodiscard]] std::shared_ptr<Value> copy_to_cell(VM& vm, const Value& value) {
    if (value.isReference()) {
        return value.asReference().value_ptr;
    }
    return vm.cell_pool.allocateCopy(value.deref());
}

[[nodiscard]] Value make_sequence_iterator(VM& vm, const Value& iterable) {
    auto iter_obj = std::make_shared<IteratorObject>(copy_to_cell(vm, iterable));
    iter_obj->cell_pool = &vm.cell_pool;
    return Value(iter_obj);
}

[[nodiscard]] bool is_builtin_sequence(const Value& value) {
    return value.isVector() || value.isString() || value.isDictionary();
}

[[nodiscard]] std::optional<FunctionType> bind_dunder(
    VM& vm,
    const Value& receiver,
    const char* name
) {
    if (auto bound = struct_try_bind_method(vm, receiver, name)) {
        return *bound;
    }
    if (auto method = lang::bind_method(copy_to_cell(vm, receiver), name)) {
        return *method;
    }
    return std::nullopt;
}

[[nodiscard]] Value make_user_next_iterator(
    VM& vm,
    const Value& receiver,
    FunctionType next_fn
) {
    auto iter_obj = std::make_shared<IteratorObject>();
    iter_obj->kind = IteratorKind::UserNext;
    iter_obj->user_next = std::move(next_fn);
    iter_obj->user_self = copy_to_cell(vm, receiver);
    iter_obj->cell_pool = &vm.cell_pool;
    return Value(iter_obj);
}

} // namespace

Value make_iter(VM& vm, const Value& obj) {
    const Value& value = obj.deref();

    if (value.isIterator()) {
        return obj;
    }

    if (value.isStruct()) {
        if (auto iter_fn = bind_dunder(vm, obj, "__iter__")) {
            const Value produced = (*iter_fn)(vm, {});
            return make_iter(vm, produced);
        }
        if (auto next_fn = bind_dunder(vm, obj, "__next__")) {
            return make_user_next_iterator(vm, obj, *next_fn);
        }
    } else if (is_builtin_sequence(value)) {
        return make_sequence_iterator(vm, obj);
    } else if (auto iter_fn = bind_dunder(vm, obj, "__iter__")) {
        const Value produced = (*iter_fn)(vm, {});
        return make_iter(vm, produced);
    } else if (auto next_fn = bind_dunder(vm, obj, "__next__")) {
        return make_user_next_iterator(vm, obj, *next_fn);
    }

    throw RuntimeError("object is not iterable");
}

bool iterator_advance(VM& vm, const Value& iter_holder, Value& out) {
    const Value& iter_val = iter_holder.deref();
    if (iter_val.getType() != Value::Type::Iterator) {
        throw RuntimeError("iterator_advance requires an iter object");
    }

    const auto iter = iter_val.asIterator();
    if (iter->kind == IteratorKind::UserNext) {
        try {
            out = iter->user_next(vm, {});
            return true;
        } catch (const RuntimeError& error) {
            if (is_stop_iteration(error)) {
                return false;
            }
            throw;
        }
    }

    return iter->next(vm, out);
}

Value iterator_next(VM& vm, const Value& iter_holder, const std::optional<Value>& default_value) {
    Value result;
    if (iterator_advance(vm, iter_holder, result)) {
        return result;
    }
    if (default_value.has_value()) {
        return *default_value;
    }
    raise_stop_iteration(vm);
}

bool IteratorObject::next(VM& vm, Value& out) {
    (void)vm;
    if (kind == IteratorKind::UserNext) {
        try {
            out = user_next(vm, {});
            return true;
        } catch (const RuntimeError& error) {
            if (is_stop_iteration(error)) {
                return false;
            }
            throw;
        }
    }

    if (!iterable) {
        return false;
    }

    Value& iterable_ref = iterable->deref();

    if (iterable_ref.isVector()) {
        auto& vec = iterable_ref.asVector();
        if (index < vec.size()) {
            if (cell_pool == nullptr) {
                throw RuntimeError("iterator has no cell pool");
            }
            out = Value::makeRef(vec[index++], *cell_pool);
            return true;
        }
    } else if (iterable_ref.isString()) {
        const std::string& str = iterable_ref.asString();
        if (index < str.size()) {
            out = Value(std::string(1, str[index++]));
            return true;
        }
    } else if (iterable_ref.isDictionary()) {
        auto& dict = iterable_ref.asDictionary();
        size_t i = 0;
        for (const auto& key : dict | std::views::keys) {
            if (i == index) {
                if (cell_pool == nullptr) {
                    throw RuntimeError("iterator has no cell pool");
                }
                out = Value::makeRef(key, *cell_pool);
                index++;
                return true;
            }
            ++i;
        }
    }

    return false;
}

} // namespace irgen
