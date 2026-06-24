#include "friend_function.hpp"

#include "struct_types.hpp"

namespace irgen {
namespace {

[[nodiscard]] bool dispatch_param_matches(const Value& arg, const std::optional<std::string>& type_name) {
    if (!type_name.has_value() || type_name->empty()) {
        return true;
    }
    try {
        check_struct_field_type(*type_name, arg);
        return true;
    } catch (const RuntimeError&) {
        return false;
    }
}

[[nodiscard]] bool handler_matches(const FunctionObject& handler, const std::vector<Value>& args) {
    if (handler.params.size() != args.size()) {
        return false;
    }
    for (size_t i = 0; i < args.size(); ++i) {
        std::optional<std::string> expected;
        if (i < handler.param_types.size()) {
            expected = handler.param_types[i];
        }
        if (!dispatch_param_matches(args[i], expected)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] int handler_specificity(const FunctionObject& handler) {
    int score = 0;
    for (const auto& param_type : handler.param_types) {
        if (param_type.has_value() && !param_type->empty()) {
            ++score;
        }
    }
    return score;
}

[[nodiscard]] std::shared_ptr<FunctionObject> find_dispatch_handler(
    const std::vector<std::shared_ptr<Value>>& handlers,
    const std::vector<Value>& args
) {
    size_t best_index = handlers.size();
    int best_score = -1;

    for (size_t i = 0; i < handlers.size(); ++i) {
        const Value& entry = handlers[i]->deref();
        if (!entry.isUserFunction()) {
            continue;
        }
        const auto& handler = *entry.asFunctionObject();
        if (!handler_matches(handler, args)) {
            continue;
        }
        const int score = handler_specificity(handler);
        if (score > best_score || (score == best_score && i > best_index)) {
            best_score = score;
            best_index = i;
        }
    }

    if (best_index >= handlers.size()) {
        return nullptr;
    }
    return handlers[best_index]->asFunctionObject();
}

} // namespace

std::shared_ptr<FunctionObject> find_convert_dispatch_handler(
    const std::vector<std::shared_ptr<Value>>& handlers,
    const std::vector<Value>& args
) {
    if (args.size() < 2) {
        return nullptr;
    }
    const Value& value = args[1];

    size_t best_index = handlers.size();
    int best_depth = -1;
    bool found = false;

    for (size_t i = 0; i < handlers.size(); ++i) {
        const Value& entry = handlers[i]->deref();
        if (!entry.isUserFunction()) {
            continue;
        }
        const auto& handler = *entry.asFunctionObject();
        if (!handler_matches(handler, args)) {
            continue;
        }
        if (handler.param_types.size() <= 1 || !handler.param_types[1].has_value()) {
            continue;
        }
        const int depth = struct_type_match_depth(value, *handler.param_types[1]);
        if (depth < 0) {
            continue;
        }
        if (!found || depth < best_depth || (depth == best_depth && i > best_index)) {
            best_depth = depth;
            best_index = i;
            found = true;
        }
    }

    if (best_index >= handlers.size()) {
        return nullptr;
    }
    return handlers[best_index]->asFunctionObject();
}

void FriendFunctionObject::ensure_dispatch_list() {
    if (!dispatch_list_holder) {
        dispatch_list_holder = std::make_shared<Value>(std::vector<std::shared_ptr<Value>>{});
    }
}

std::vector<std::shared_ptr<Value>>& FriendFunctionObject::dispatch_handlers() {
    ensure_dispatch_list();
    return dispatch_list_holder->asVector();
}

std::shared_ptr<FriendFunctionObject> make_friend_function(std::string name) {
    auto obj = std::make_shared<FriendFunctionObject>();
    obj->name = std::move(name);
    obj->ensure_dispatch_list();
    return obj;
}

std::optional<Value> friend_get_attr(
    VM& vm,
    const std::shared_ptr<FriendFunctionObject>& obj,
    const std::string& attr_name
) {
    if (attr_name == "__dispatch__") {
        obj->ensure_dispatch_list();
        return Value::makeRef(obj->dispatch_list_holder, vm.cell_pool);
    }
    return std::nullopt;
}

void friend_invoke_dispatch(
    VM& vm,
    const std::shared_ptr<FriendFunctionObject>& obj,
    const std::vector<Value>& positional,
    const Value& kwargs_value
) {
    obj->ensure_dispatch_list();
    const auto& handlers = obj->dispatch_handlers();
    const std::shared_ptr<FunctionObject> handler =
        obj->name == "__convert__"
            ? find_convert_dispatch_handler(handlers, positional)
            : find_dispatch_handler(handlers, positional);
    if (handler == nullptr) {
        if (obj->name == "__convert__") {
            throw RuntimeError(
                std::format(
                    "type __convert__ has no matching __dispatch__ implementation for {} argument(s)",
                    positional.size()
                )
            );
        }
        throw RuntimeError(
            std::format(
                "friend func '{}' has no matching __dispatch__ implementation for {} argument(s)",
                obj->name,
                positional.size()
            )
        );
    }

    if (!handler->owner_vm) {
        handler->owner_vm = &vm;
    }

    std::vector<Value> resolved = resolve_user_function_args(vm, *handler, positional, kwargs_value);
    invoke_user_function_with_args(vm, handler, std::move(resolved));
}

} // namespace irgen
