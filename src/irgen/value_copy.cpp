#include "value_copy.hpp"

namespace irgen {
namespace {

[[nodiscard]] Value detach_value_impl(const Value& value) {
    const Value& v = value.deref();

    if (v.isReference()) {
        return detach_value_impl(v);
    }
    if (v.isNone() || v.isBool() || v.isNumber() || v.isString() || v.isRational()) {
        return v;
    }
    if (v.isVector()) {
        std::vector<std::shared_ptr<Value>> copy;
        copy.reserve(v.asVector().size());
        for (const auto& elem : v.asVector()) {
            if (elem) {
                copy.push_back(std::make_shared<Value>(detach_value_impl(*elem)));
            } else {
                copy.push_back(std::make_shared<Value>());
            }
        }
        return Value(std::move(copy));
    }
    if (v.isDictionary()) {
        std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> copy;
        for (const auto& [key, val] : v.asDictionary()) {
            copy[std::make_shared<Value>(detach_value_impl(*key))] =
                std::make_shared<Value>(detach_value_impl(*val));
        }
        return Value(std::move(copy));
    }
    if (v.isStruct()) {
        const auto inst = v.asStruct();
        auto copy = std::make_shared<StructObject>();
        copy->type = inst->type;
        copy->slots.reserve(inst->slots.size());
        for (const auto& slot : inst->slots) {
            copy->slots.push_back(detach_value_impl(slot));
        }
        return Value(copy);
    }

    return v;
}

} // namespace

Value detach_value(const Value& value) {
    return detach_value_impl(value);
}

} // namespace irgen
