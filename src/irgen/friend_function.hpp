#pragma once

#include "opcode.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace irgen {

struct FriendFunctionObject {
    std::string name;
    std::shared_ptr<Value> dispatch_list_holder;

    void ensure_dispatch_list();

    [[nodiscard]] std::vector<std::shared_ptr<Value>>& dispatch_handlers();
};

[[nodiscard]] std::shared_ptr<FriendFunctionObject> make_friend_function(std::string name);

[[nodiscard]] std::optional<Value> friend_get_attr(
    VM& vm,
    const std::shared_ptr<FriendFunctionObject>& obj,
    const std::string& attr_name
);

void friend_invoke_dispatch(
    VM& vm,
    const std::shared_ptr<FriendFunctionObject>& obj,
    const std::vector<Value>& positional,
    const Value& kwargs_value
);

} // namespace irgen
