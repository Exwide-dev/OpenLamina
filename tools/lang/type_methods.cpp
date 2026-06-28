#include "type_methods.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "irgen/iterator_ops.hpp"
#include "irgen/opcode.hpp"

namespace lang {
namespace {

using irgen::Value;
using irgen::VM;

[[nodiscard]] Value& slot_value(const std::shared_ptr<Value>& receiver) {
    if (!receiver) {
        throw RuntimeError("method receiver is null");
    }
    return *receiver;
}

[[nodiscard]] std::shared_ptr<Value> dict_find_key_ptr(
    std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>& dict,
    const Value& key
) {
    const Value& key_deref = key.deref();
    for (const auto& tkey : dict | std::views::keys) {
        if (*tkey == key_deref) {
            return tkey;
        }
    }
    return nullptr;
}

} // namespace

std::optional<irgen::FunctionType> bind_method(
    const std::shared_ptr<Value>& receiver,
    const std::string& method_name
) {
    if (!receiver) {
        return std::nullopt;
    }

    if (method_name == "displayString") {
        return [receiver](VM&, const std::vector<Value>& args) -> Value {
            (void)args;
            return Value(slot_value(receiver).deref().displayString());
        };
    }
    if (method_name == "printString") {
        return [receiver](VM&, const std::vector<Value>& args) -> Value {
            (void)args;
            return Value(slot_value(receiver).deref().printString());
        };
    }

    Value& self = *receiver;

    if (self.isVector()) {
        if (method_name == "append") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("append requires 1 argument");
                }
                auto& vec = slot_value(receiver).asVector();
                vec.push_back(vm.cell_pool.allocateCopy(args[0].deref()));
                return {};
            };
        }
        if (method_name == "extend") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("extend requires 1 argument");
                }
                const Value& other = args[0].deref();
                if (!other.isVector()) {
                    throw RuntimeError("extend requires a vector argument");
                }
                auto& vec = slot_value(receiver).asVector();
                for (const auto& elem : other.asVector()) {
                    vec.push_back(vm.cell_pool.allocateCopy(*elem));
                }
                return {};
            };
        }
        if (method_name == "pop") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                auto& vec = slot_value(receiver).asVector();
                if (vec.empty()) {
                    throw RuntimeError("pop from empty vector");
                }
                if (!args.empty()) {
                    const Value& idx_val = args[0].deref();
                    if (!idx_val.isNumber()) {
                        throw RuntimeError("pop index must be a number");
                    }
                    const ptrdiff_t idx = idx_val.asInt();
                    if (idx < 0 || static_cast<size_t>(idx) >= vec.size()) {
                        throw RuntimeError("pop index out of range");
                    }
                    Value result = *vec[static_cast<size_t>(idx)];
                    vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(idx));
                    return result;
                }
                Value result = *vec.back();
                vec.pop_back();
                return result;
            };
        }
        if (method_name == "clear") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                slot_value(receiver).asVector().clear();
                return {};
            };
        }
        if (method_name == "len") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                return Value(static_cast<int64_t>(slot_value(receiver).asVector().size()));
            };
        }
        if (method_name == "join") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("join requires 1 argument");
                }
                const Value& sep_val = args[0].deref();
                if (!sep_val.isString()) {
                    throw RuntimeError("join separator must be text");
                }
                const std::string& sep = sep_val.asString();
                std::string result;
                bool first = true;
                for (const auto& elem : slot_value(receiver).asVector()) {
                    if (!first) {
                        result += sep;
                    }
                    first = false;
                    result += elem->deref().printString();
                }
                return Value(std::move(result));
            };
        }
        if (method_name == "contains") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("contains requires 1 argument");
                }
                const Value& needle = args[0].deref();
                for (const auto& elem : slot_value(receiver).asVector()) {
                    if (*elem == needle) {
                        return Value(true);
                    }
                }
                return Value(false);
            };
        }
        if (method_name == "reverse") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                auto& vec = slot_value(receiver).asVector();
                std::ranges::reverse(vec);
                return {};
            };
        }
        if (method_name == "get") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("get requires 1 argument");
                }
                const Value& idx_val = args[0].deref();
                if (!idx_val.isNumber()) {
                    throw RuntimeError("get index must be a number");
                }
                const ptrdiff_t idx = idx_val.asInt();
                auto& vec = slot_value(receiver).asVector();
                if (idx < 0 || static_cast<size_t>(idx) >= vec.size()) {
                    throw RuntimeError("vector index out of range");
                }
                return *vec[static_cast<size_t>(idx)];
            };
        }
        if (method_name == "set") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                if (args.size() != 2) {
                    throw RuntimeError("set requires 2 arguments");
                }
                const Value& idx_val = args[0].deref();
                if (!idx_val.isNumber()) {
                    throw RuntimeError("set index must be a number");
                }
                const ptrdiff_t idx = idx_val.asInt();
                auto& vec = slot_value(receiver).asVector();
                if (idx < 0 || static_cast<size_t>(idx) >= vec.size()) {
                    throw RuntimeError("vector index out of range");
                }
                vec[static_cast<size_t>(idx)] = vm.cell_pool.allocateCopy(args[1].deref());
                return {};
            };
        }
        if (method_name == "insert") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                if (args.size() != 2) {
                    throw RuntimeError("insert requires 2 arguments");
                }
                const Value& idx_val = args[0].deref();
                if (!idx_val.isNumber()) {
                    throw RuntimeError("insert index must be a number");
                }
                const ptrdiff_t idx = idx_val.asInt();
                auto& vec = slot_value(receiver).asVector();
                if (idx < 0 || static_cast<size_t>(idx) > vec.size()) {
                    throw RuntimeError("insert index out of range");
                }
                vec.insert(
                    vec.begin() + static_cast<std::ptrdiff_t>(idx),
                    vm.cell_pool.allocateCopy(args[1].deref())
                );
                return {};
            };
        }
        if (method_name == "remove_at") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("remove_at requires 1 argument");
                }
                const Value& idx_val = args[0].deref();
                if (!idx_val.isNumber()) {
                    throw RuntimeError("remove_at index must be a number");
                }
                const ptrdiff_t idx = idx_val.asInt();
                auto& vec = slot_value(receiver).asVector();
                if (idx < 0 || static_cast<size_t>(idx) >= vec.size()) {
                    throw RuntimeError("remove_at index out of range");
                }
                Value result = *vec[static_cast<size_t>(idx)];
                vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(idx));
                return result;
            };
        }
        if (method_name == "remove") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("remove requires 1 argument");
                }
                const Value& needle = args[0].deref();
                auto& vec = slot_value(receiver).asVector();
                for (auto it = vec.begin(); it != vec.end(); ++it) {
                    if (**it == needle) {
                        Value result = **it;
                        vec.erase(it);
                        return result;
                    }
                }
                throw RuntimeError("value not found in vector");
            };
        }
        if (method_name == "index") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("index requires 1 argument");
                }
                const Value& needle = args[0].deref();
                const auto& vec = slot_value(receiver).asVector();
                for (size_t i = 0; i < vec.size(); ++i) {
                    if (*vec[i] == needle) {
                        return Value(static_cast<int64_t>(i));
                    }
                }
                return Value(static_cast<int64_t>(-1));
            };
        }
        if (method_name == "first") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                const auto& vec = slot_value(receiver).asVector();
                if (vec.empty()) {
                    throw RuntimeError("first on empty vector");
                }
                return *vec.front();
            };
        }
        if (method_name == "last") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                const auto& vec = slot_value(receiver).asVector();
                if (vec.empty()) {
                    throw RuntimeError("last on empty vector");
                }
                return *vec.back();
            };
        }
        if (method_name == "empty") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                return Value(slot_value(receiver).asVector().empty());
            };
        }
        if (method_name == "copy") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                (void)args;
                std::vector<std::shared_ptr<Value>> out;
                for (const auto& elem : slot_value(receiver).asVector()) {
                    out.push_back(vm.cell_pool.allocateCopy(*elem));
                }
                return Value(std::move(out));
            };
        }
        if (method_name == "sort") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                auto& vec = slot_value(receiver).asVector();
                std::ranges::sort(vec, [](const std::shared_ptr<Value>& a, const std::shared_ptr<Value>& b) {
                    return a->deref().printString() < b->deref().printString();
                });
                return {};
            };
        }
        return std::nullopt;
    }

    if (self.isDictionary()) {
        if (method_name == "get") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.empty() || args.size() > 2) {
                    throw RuntimeError("get requires 1 or 2 arguments");
                }
                auto& dict = slot_value(receiver).asDictionary();
                const Value& key = args[0].deref();
                for (const auto& [tkey, tval] : dict) {
                    if (*tkey == key) {
                        return *tval;
                    }
                }
                if (args.size() == 2) {
                    return args[1].deref();
                }
                throw RuntimeError("key " + key.toString() + " not found");
            };
        }
        if (method_name == "set") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                if (args.size() != 2) {
                    throw RuntimeError("set requires 2 arguments");
                }
                auto& dict = slot_value(receiver).asDictionary();
                const Value& key = args[0].deref();
                if (auto existing = dict_find_key_ptr(dict, key)) {
                    *dict[existing] = args[1].deref();
                } else {
                    dict[vm.cell_pool.allocateCopy(key)] = vm.cell_pool.allocateCopy(args[1].deref());
                }
                return {};
            };
        }
        if (method_name == "pop") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.empty() || args.size() > 2) {
                    throw RuntimeError("pop requires 1 or 2 arguments");
                }
                auto& dict = slot_value(receiver).asDictionary();
                const Value& key = args[0].deref();
                auto existing = dict_find_key_ptr(dict, key);
                if (!existing) {
                    if (args.size() == 2) {
                        return args[1].deref();
                    }
                    throw RuntimeError("key " + key.toString() + " not found");
                }
                Value result = *dict[existing];
                dict.erase(existing);
                return result;
            };
        }
        if (method_name == "clear") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                slot_value(receiver).asDictionary().clear();
                return {};
            };
        }
        if (method_name == "len") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                return Value(static_cast<int64_t>(slot_value(receiver).asDictionary().size()));
            };
        }
        if (method_name == "contains" || method_name == "has") {
            const std::string method = method_name;
            return [receiver, method](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError(method + " requires 1 argument");
                }
                auto& dict = slot_value(receiver).asDictionary();
                return Value(dict_find_key_ptr(dict, args[0].deref()) != nullptr);
            };
        }
        if (method_name == "keys") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                (void)args;
                std::vector<std::shared_ptr<Value>> keys;
                for (const auto& k : slot_value(receiver).asDictionary() | std::views::keys) {
                    keys.push_back(vm.cell_pool.allocateCopy(*k));
                }
                return Value(std::move(keys));
            };
        }
        if (method_name == "values") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                (void)args;
                std::vector<std::shared_ptr<Value>> values;
                for (const auto& v : slot_value(receiver).asDictionary() | std::views::values) {
                    values.push_back(vm.cell_pool.allocateCopy(*v));
                }
                return Value(std::move(values));
            };
        }
        if (method_name == "update") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("update requires 1 argument");
                }
                const Value& other = args[0].deref();
                if (!other.isDictionary()) {
                    throw RuntimeError("update requires a dictionary argument");
                }
                auto& dict = slot_value(receiver).asDictionary();
                for (const auto& [k, v] : other.asDictionary()) {
                    if (auto existing = dict_find_key_ptr(dict, *k)) {
                        *dict[existing] = *v;
                    } else {
                        dict[vm.cell_pool.allocateCopy(*k)] = vm.cell_pool.allocateCopy(*v);
                    }
                }
                return {};
            };
        }
        if (method_name == "put") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                if (args.size() != 2) {
                    throw RuntimeError("put requires 2 arguments");
                }
                auto& dict = slot_value(receiver).asDictionary();
                const Value& key = args[0].deref();
                if (auto existing = dict_find_key_ptr(dict, key)) {
                    *dict[existing] = args[1].deref();
                } else {
                    dict[vm.cell_pool.allocateCopy(key)] = vm.cell_pool.allocateCopy(args[1].deref());
                }
                return {};
            };
        }
        if (method_name == "remove") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("remove requires 1 argument");
                }
                auto& dict = slot_value(receiver).asDictionary();
                const Value& key = args[0].deref();
                auto existing = dict_find_key_ptr(dict, key);
                if (!existing) {
                    throw RuntimeError("key " + key.toString() + " not found");
                }
                Value result = *dict[existing];
                dict.erase(existing);
                return result;
            };
        }
        if (method_name == "items") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                (void)args;
                std::vector<std::shared_ptr<Value>> pairs;
                for (const auto& [k, v] : slot_value(receiver).asDictionary()) {
                    pairs.push_back(vm.cell_pool.allocateCopy(
                        Value(std::vector<std::shared_ptr<Value>>{
                            vm.cell_pool.allocateCopy(*k),
                            vm.cell_pool.allocateCopy(*v),
                        })
                    ));
                }
                return Value(std::move(pairs));
            };
        }
        return std::nullopt;
    }

    if (self.isString()) {
        if (method_name == "upper") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                std::string s = slot_value(receiver).asString();
                std::ranges::transform(s, s.begin(), [](const unsigned char c) {
                    return static_cast<char>(std::toupper(c));
                });
                return Value(std::move(s));
            };
        }
        if (method_name == "lower") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                std::string s = slot_value(receiver).asString();
                std::ranges::transform(s, s.begin(), [](const unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                return Value(std::move(s));
            };
        }
        if (method_name == "strip") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                std::string s = slot_value(receiver).asString();
                const auto start = s.find_first_not_of(" \t\r\n");
                if (start == std::string::npos) {
                    return Value(std::string{});
                }
                const auto end = s.find_last_not_of(" \t\r\n");
                return Value(s.substr(start, end - start + 1));
            };
        }
        if (method_name == "split") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                std::string sep = " ";
                if (!args.empty()) {
                    const Value& sep_val = args[0].deref();
                    if (!sep_val.isString()) {
                        throw RuntimeError("split separator must be a string");
                    }
                    sep = sep_val.asString();
                }
                const std::string& s = slot_value(receiver).asString();
                std::vector<std::shared_ptr<Value>> parts;
                if (sep.empty()) {
                    throw RuntimeError("split separator must not be empty");
                }
                size_t pos = 0;
                while (pos <= s.size()) {
                    const size_t found = s.find(sep, pos);
                    if (found == std::string::npos) {
                        parts.push_back(vm.cell_pool.allocateValue(Value(s.substr(pos))));
                        break;
                    }
                    parts.push_back(vm.cell_pool.allocateValue(Value(s.substr(pos, found - pos))));
                    pos = found + sep.size();
                }
                return Value(std::move(parts));
            };
        }
        if (method_name == "contains") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("contains requires 1 argument");
                }
                const Value& needle = args[0].deref();
                if (!needle.isString()) {
                    throw RuntimeError("contains requires a string argument");
                }
                const std::string& hay = slot_value(receiver).asString();
                return Value(hay.find(needle.asString()) != std::string::npos);
            };
        }
        if (method_name == "startswith") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("startswith requires 1 argument");
                }
                const Value& prefix = args[0].deref();
                if (!prefix.isString()) {
                    throw RuntimeError("startswith requires a string argument");
                }
                const std::string& s = slot_value(receiver).asString();
                const std::string& p = prefix.asString();
                return Value(s.size() >= p.size() && s.compare(0, p.size(), p) == 0);
            };
        }
        if (method_name == "endswith") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw RuntimeError("endswith requires 1 argument");
                }
                const Value& suffix = args[0].deref();
                if (!suffix.isString()) {
                    throw RuntimeError("endswith requires a string argument");
                }
                const std::string& s = slot_value(receiver).asString();
                const std::string& suf = suffix.asString();
                return Value(s.size() >= suf.size() &&
                             s.compare(s.size() - suf.size(), suf.size(), suf) == 0);
            };
        }
        if (method_name == "len") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                return Value(static_cast<int64_t>(slot_value(receiver).asString().size()));
            };
        }
        return std::nullopt;
    }

    if (self.isNumber()) {
        if (method_name == "abs") {
            return [receiver](VM&, const std::vector<Value>& args) -> Value {
                (void)args;
                auto n = slot_value(receiver).asNumber();
                if (n.isNegative()) {
                    return Value(-n);
                }
                return Value(n);
            };
        }
        return std::nullopt;
    }

    if (self.isIterator()) {
        if (method_name == "__next__") {
            return [receiver](VM& vm, const std::vector<Value>& args) -> Value {
                (void)args;
                return iterator_next(vm, Value(*receiver));
            };
        }
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace lang
