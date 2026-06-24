#include "bytecode_file.hpp"

#include "friend_function.hpp"
#include "generator.hpp"
#include "runtime_ast.hpp"
#include "struct_types.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include <variant>

namespace lm::irgen {
namespace {

using Opcode = ::irgen::Opcode;
using Value = ::irgen::Value;
using FunctionObject = ::irgen::FunctionObject;
using StructTypeDef = ::irgen::StructTypeDef;
using StructFieldDef = ::irgen::StructFieldDef;
using RuntimeAstNode = ::irgen::RuntimeAstNode;
using FriendFunctionObject = ::irgen::FriendFunctionObject;

constexpr char LMC_MAGIC[4] = {'L', 'M', 'C', '\x01'};
constexpr uint32_t LMC_FLAG_OPTIMIZED = 1u;

enum class ValueTag : uint8_t {
    None = 0,
    Number = 1,
    Bool = 2,
    String = 3,
    Rational = 4,
    Vector = 5,
    UserFunction = 6,
    TypeHandle = 7,
    FriendFunction = 8,
    RuntimeAst = 9,
};

class ByteWriter {
    std::vector<uint8_t> buf_;

public:
    [[nodiscard]] const std::vector<uint8_t>& data() const { return buf_; }

    void write_bytes(const void* ptr, const size_t n) {
        const auto* p = static_cast<const uint8_t*>(ptr);
        buf_.insert(buf_.end(), p, p + n);
    }

    void write_u8(const uint8_t v) { write_bytes(&v, 1); }

    void write_u32(const uint32_t v) {
        const uint8_t b[4] = {
            static_cast<uint8_t>(v),
            static_cast<uint8_t>(v >> 8),
            static_cast<uint8_t>(v >> 16),
            static_cast<uint8_t>(v >> 24),
        };
        write_bytes(b, 4);
    }

    void write_u64(const uint64_t v) {
        uint8_t b[8]{};
        for (int i = 0; i < 8; ++i) {
            b[i] = static_cast<uint8_t>(v >> (8 * i));
        }
        write_bytes(b, 8);
    }

    void write_string(const std::string& s) {
        write_u64(s.size());
        if (!s.empty()) {
            write_bytes(s.data(), s.size());
        }
    }

    void write_bool(const bool v) { write_u8(v ? 1u : 0u); }
};

class ByteReader {
    const std::vector<uint8_t>& buf_;
    size_t pos_ = 0;

public:
    explicit ByteReader(const std::vector<uint8_t>& buf) : buf_(buf) {}

    [[nodiscard]] size_t remaining() const { return buf_.size() - pos_; }

    void read_bytes(void* dst, const size_t n) {
        if (pos_ + n > buf_.size()) {
            throw std::runtime_error("LMC: unexpected end of file");
        }
        std::memcpy(dst, buf_.data() + pos_, n);
        pos_ += n;
    }

    [[nodiscard]] uint8_t read_u8() {
        uint8_t v = 0;
        read_bytes(&v, 1);
        return v;
    }

    [[nodiscard]] uint32_t read_u32() {
        uint8_t b[4]{};
        read_bytes(b, 4);
        return static_cast<uint32_t>(b[0]) |
               (static_cast<uint32_t>(b[1]) << 8) |
               (static_cast<uint32_t>(b[2]) << 16) |
               (static_cast<uint32_t>(b[3]) << 24);
    }

    [[nodiscard]] uint64_t read_u64() {
        uint8_t b[8]{};
        read_bytes(b, 8);
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<uint64_t>(b[i]) << (8 * i);
        }
        return v;
    }

    [[nodiscard]] std::string read_string() {
        const uint64_t len = read_u64();
        if (len == 0) {
            return {};
        }
        if (len > remaining()) {
            throw std::runtime_error("LMC: invalid string length");
        }
        std::string s(static_cast<size_t>(len), '\0');
        read_bytes(s.data(), static_cast<size_t>(len));
        return s;
    }

    [[nodiscard]] bool read_bool() { return read_u8() != 0; }
};

void write_value(ByteWriter& w, const Value& val);
Value read_value(ByteReader& r);
void write_runtime_ast(ByteWriter& w, const RuntimeAstNode& node);
RuntimeAstNode read_runtime_ast(ByteReader& r);
void write_function(ByteWriter& w, const FunctionObject& func);
std::shared_ptr<FunctionObject> read_function(ByteReader& r);
void write_friend_function(ByteWriter& w, const FriendFunctionObject& obj);
std::shared_ptr<FriendFunctionObject> read_friend_function(ByteReader& r);
void write_struct_field(ByteWriter& w, const StructFieldDef& field);
StructFieldDef read_struct_field(ByteReader& r);
void write_type_def(ByteWriter& w, const StructTypeDef& def);
StructTypeDef read_type_def(ByteReader& r);
void write_opcode(ByteWriter& w, const Opcode& op);
Opcode read_opcode(ByteReader& r);
void write_opcodes(ByteWriter& w, const std::vector<Opcode>& code);
std::vector<Opcode> read_opcodes(ByteReader& r);

void optimize_function_object(const std::shared_ptr<FunctionObject>& func) {
    if (!func) {
        return;
    }
    (void)optimize_bytecode(func->body);
    for (auto& ir : func->param_default_ir) {
        (void)optimize_bytecode(ir);
    }
}

void optimize_value_tree(Value& val) {
    if (val.isUserFunction()) {
        optimize_function_object(val.asFunctionObject());
        return;
    }
    if (val.isTypeHandle()) {
        auto& def = val.asTypeDef();
        for (auto& [_, method] : def->methods) {
            optimize_function_object(method);
        }
        if (def->convert_func) {
            for (auto& handler : def->convert_func->dispatch_handlers()) {
                if (handler && handler->isUserFunction()) {
                    optimize_function_object(handler->asFunctionObject());
                }
            }
        }
        return;
    }
    if (val.isFriendFunction()) {
        auto& obj = val.asFriendFunction();
        for (auto& handler : obj->dispatch_handlers()) {
            if (handler && handler->isUserFunction()) {
                optimize_function_object(handler->asFunctionObject());
            }
        }
        return;
    }
    if (val.isVector()) {
        for (auto& elem : val.asVector()) {
            if (elem) {
                optimize_value_tree(*elem);
            }
        }
    }
}

void collect_user_type_defs_from_value(
    const Value& val,
    std::unordered_set<std::string>& seen,
    std::vector<StructTypeDef>& out
) {
    if (val.isTypeHandle()) {
        const auto& def = val.asTypeDef();
        if (def->kind == ::irgen::TypeKind::User && !seen.contains(def->name)) {
            seen.insert(def->name);
            out.push_back(*def);
            for (const auto& [_, method] : def->methods) {
                if (!method) {
                    continue;
                }
                for (const auto& ir : method->param_default_ir) {
                    for (const auto& op : ir) {
                        if (const auto* push = std::get_if<::irgen::PUSH>(&op)) {
                            collect_user_type_defs_from_value(push->val, seen, out);
                        }
                    }
                }
                for (const auto& op : method->body) {
                    if (const auto* push = std::get_if<::irgen::PUSH>(&op)) {
                        collect_user_type_defs_from_value(push->val, seen, out);
                    }
                }
            }
        }
        return;
    }
    if (val.isUserFunction()) {
        const auto& func = val.asFunctionObject();
        for (const auto& ir : func->param_default_ir) {
            for (const auto& op : ir) {
                if (const auto* push = std::get_if<::irgen::PUSH>(&op)) {
                    collect_user_type_defs_from_value(push->val, seen, out);
                }
            }
        }
        for (const auto& op : func->body) {
            if (const auto* push = std::get_if<::irgen::PUSH>(&op)) {
                collect_user_type_defs_from_value(push->val, seen, out);
            }
        }
        return;
    }
    if (val.isFriendFunction()) {
        for (const auto& handler : val.asFriendFunction()->dispatch_handlers()) {
            if (handler) {
                collect_user_type_defs_from_value(*handler, seen, out);
            }
        }
        return;
    }
    if (val.isVector()) {
        for (const auto& elem : val.asVector()) {
            if (elem) {
                collect_user_type_defs_from_value(*elem, seen, out);
            }
        }
    }
}

std::vector<StructTypeDef> collect_user_type_defs(const std::vector<Opcode>& code) {
    std::unordered_set<std::string> seen;
    std::vector<StructTypeDef> out;
    for (const auto& op : code) {
        if (const auto* push = std::get_if<::irgen::PUSH>(&op)) {
            collect_user_type_defs_from_value(push->val, seen, out);
        }
    }
    return out;
}

void write_opcode(ByteWriter& w, const Opcode& op) {
    w.write_u8(static_cast<uint8_t>(op.index()));
    std::visit(
        [&](const auto& instruction) {
            using T = std::decay_t<decltype(instruction)>;
            if constexpr (std::is_same_v<T, ::irgen::PUSH>) {
                write_value(w, instruction.val);
            } else if constexpr (std::is_same_v<T, ::irgen::LOAD> ||
                                 std::is_same_v<T, ::irgen::STORE_ARG> ||
                                 std::is_same_v<T, ::irgen::NEW_VAR> ||
                                 std::is_same_v<T, ::irgen::NEW_CONST> ||
                                 std::is_same_v<T, ::irgen::NEW_INTERN_VAR> ||
                                 std::is_same_v<T, ::irgen::NEW_INTERN_CONST> ||
                                 std::is_same_v<T, ::irgen::NEW_VAR_OR_LOAD>) {
                w.write_u64(instruction.var_id);
            } else if constexpr (std::is_same_v<T, ::irgen::FINDMOD>) {
                w.write_u64(instruction.module_id);
            } else if constexpr (std::is_same_v<T, ::irgen::GETATTR> ||
                                 std::is_same_v<T, ::irgen::SET_FIELD>) {
                w.write_u64(instruction.name_id);
            } else if constexpr (std::is_same_v<T, ::irgen::EXC_MATCH> ||
                                 std::is_same_v<T, ::irgen::IS_INSTANCE>) {
                w.write_u64(instruction.type_name_id);
            } else if constexpr (std::is_same_v<T, ::irgen::LABEL> ||
                                 std::is_same_v<T, ::irgen::GOTO> ||
                                 std::is_same_v<T, ::irgen::GOTOIF> ||
                                 std::is_same_v<T, ::irgen::GOTOIFNOT>) {
                w.write_u64(instruction.label_id);
            } else if constexpr (std::is_same_v<T, ::irgen::CALL>) {
                w.write_u64(instruction.arg_count);
                w.write_bool(instruction.has_kwargs);
                w.write_u64(instruction.splat_mask);
            } else if constexpr (std::is_same_v<T, ::irgen::LOAD_FAST> ||
                                 std::is_same_v<T, ::irgen::STORE_FAST>) {
                w.write_u64(instruction.slot);
            } else if constexpr (std::is_same_v<T, ::irgen::BIND_FAST>) {
                w.write_u64(instruction.slot);
                w.write_u64(instruction.var_id);
            } else if constexpr (std::is_same_v<T, ::irgen::VEC_NEW> ||
                                 std::is_same_v<T, ::irgen::DICT_NEW>) {
                w.write_u64(instruction.count);
            } else if constexpr (std::is_same_v<T, ::irgen::STRUCT_NEW>) {
                w.write_u64(instruction.struct_id);
                w.write_u64(instruction.arg_count);
            } else if constexpr (std::is_same_v<T, ::irgen::ENTER_TRY>) {
                w.write_u64(instruction.catch_label);
                w.write_u64(instruction.else_label);
                w.write_u64(instruction.end_label);
            }
        },
        op
    );
}

void write_opcodes(ByteWriter& w, const std::vector<Opcode>& code) {
    w.write_u64(code.size());
    for (const auto& op : code) {
        write_opcode(w, op);
    }
}

void write_value(ByteWriter& w, const Value& val) {
    if (val.isNone()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::None));
        return;
    }
    if (val.isNumber()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::Number));
        w.write_string(val.asNumber().toString());
        return;
    }
    if (val.isBool()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::Bool));
        w.write_bool(val.asBool());
        return;
    }
    if (val.isString()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::String));
        w.write_string(val.asString());
        return;
    }
    if (val.isRational()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::Rational));
        const auto& rat = val.asRational();
        w.write_string(rat.numerator().toString());
        w.write_string(rat.denominator().toString());
        return;
    }
    if (val.isVector()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::Vector));
        const auto& vec = val.asVector();
        w.write_u64(vec.size());
        for (const auto& elem : vec) {
            write_value(w, elem ? *elem : Value());
        }
        return;
    }
    if (val.isUserFunction()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::UserFunction));
        write_function(w, *val.asFunctionObject());
        return;
    }
    if (val.isTypeHandle()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::TypeHandle));
        w.write_string(val.asTypeDef()->name);
        return;
    }
    if (val.isFriendFunction()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::FriendFunction));
        write_friend_function(w, *val.asFriendFunction());
        return;
    }
    if (val.isRuntimeAst()) {
        w.write_u8(static_cast<uint8_t>(ValueTag::RuntimeAst));
        write_runtime_ast(w, val.asRuntimeAst());
        return;
    }
    throw std::runtime_error("LMC: cannot serialize runtime value type: " + val.type_name());
}

void write_function(ByteWriter& w, const FunctionObject& func) {
    w.write_string(func.name);
    w.write_u64(func.params.size());
    for (const auto& p : func.params) {
        w.write_string(p);
    }
    w.write_u64(func.param_types.size());
    for (const auto& pt : func.param_types) {
        if (pt) {
            w.write_bool(true);
            w.write_string(*pt);
        } else {
            w.write_bool(false);
        }
    }
    w.write_u64(func.param_default_ir.size());
    for (const auto& ir : func.param_default_ir) {
        write_opcodes(w, ir);
    }
    write_opcodes(w, func.body);
    w.write_u64(func.location);
    w.write_bool(func.needs_closure);
    w.write_bool(func.needs_symbol_bind);
    w.write_bool(func.is_macro);
    if (func.variadic_param_index) {
        w.write_bool(true);
        w.write_u64(*func.variadic_param_index);
    } else {
        w.write_bool(false);
    }
}

void write_friend_function(ByteWriter& w, const FriendFunctionObject& obj) {
    w.write_string(obj.name);
    auto& handlers = const_cast<FriendFunctionObject&>(obj).dispatch_handlers();
    w.write_u64(handlers.size());
    for (const auto& handler : handlers) {
        if (!handler || !handler->isUserFunction()) {
            throw std::runtime_error("LMC: friend function handler must be a user function");
        }
        write_function(w, *handler->asFunctionObject());
    }
}

void write_runtime_ast(ByteWriter& w, const RuntimeAstNode& node) {
    w.write_u32(static_cast<uint32_t>(node.kind));
    w.write_u32(static_cast<uint32_t>(node.line));
    w.write_string(node.text);
    w.write_bool(node.bool_val);

    w.write_u64(node.stmts.size());
    for (const auto& stmt : node.stmts) {
        write_runtime_ast(w, stmt);
    }
    w.write_u64(node.children.size());
    for (const auto& child : node.children) {
        write_runtime_ast(w, child);
    }

    const auto write_optional = [&](const std::unique_ptr<RuntimeAstNode>& slot) {
        if (slot) {
            w.write_bool(true);
            write_runtime_ast(w, *slot);
        } else {
            w.write_bool(false);
        }
    };
    write_optional(node.slot_a);
    write_optional(node.slot_b);
    write_optional(node.slot_c);

    w.write_u64(node.hygienic_names.size());
    for (const auto& n : node.hygienic_names) {
        w.write_string(n);
    }
    w.write_u64(node.binding_names.size());
    for (const auto& n : node.binding_names) {
        w.write_string(n);
    }
    w.write_u64(node.bindings.size());
    for (const auto& b : node.bindings) {
        write_runtime_ast(w, b);
    }

    w.write_u64(node.call_args.size());
    for (const auto& arg : node.call_args) {
        w.write_string(arg.kw_name);
        w.write_bool(arg.is_splat);
        if (arg.value) {
            w.write_bool(true);
            write_runtime_ast(w, *arg.value);
        } else {
            w.write_bool(false);
        }
    }
}

void write_struct_field(ByteWriter& w, const StructFieldDef& field) {
    w.write_string(field.name);
    w.write_string(field.type_name);
    w.write_bool(field.has_type_annotation);
    w.write_bool(field.mutable_field);
    w.write_bool(field.has_default);
    if (field.has_default) {
        write_value(w, field.default_value);
    }
}

void write_type_def(ByteWriter& w, const StructTypeDef& def) {
    w.write_string(def.name);
    w.write_string(def.base_name);
    w.write_u8(static_cast<uint8_t>(def.kind));
    w.write_bool(def.typed);
    w.write_u64(def.fields.size());
    for (const auto& field : def.fields) {
        write_struct_field(w, field);
    }
    w.write_u64(def.methods.size());
    for (const auto& [name, method] : def.methods) {
        w.write_string(name);
        write_function(w, *method);
    }
    if (def.convert_func && !def.convert_func->dispatch_handlers().empty()) {
        w.write_bool(true);
        write_friend_function(w, *def.convert_func);
    } else {
        w.write_bool(false);
    }
}

Opcode read_opcode(ByteReader& r) {
    const uint8_t tag = r.read_u8();
    switch (tag) {
        case 0:
            return Opcode(::irgen::PUSH(read_value(r)));
        case 1:
            return Opcode(::irgen::ADD{});
        case 2:
            return Opcode(::irgen::MUL{});
        case 3:
            return Opcode(::irgen::SUB{});
        case 4:
            return Opcode(::irgen::DIV{});
        case 5:
            return Opcode(::irgen::NEG{});
        case 6:
            return Opcode(::irgen::DEREF{});
        case 7:
            return Opcode(::irgen::ADDR_OF{});
        case 8:
            return Opcode(::irgen::DEREF_PTR{});
        case 9:
            return Opcode(::irgen::PTR_TO_REF{});
        case 10:
            return Opcode(::irgen::NOT{});
        case 11:
            return Opcode(::irgen::TRUTHY_NOT{});
        case 12:
            return Opcode(::irgen::AND{});
        case 13:
            return Opcode(::irgen::OR{});
        case 14:
            return Opcode(::irgen::EQ{});
        case 15:
            return Opcode(::irgen::NEQ{});
        case 16:
            return Opcode(::irgen::LT{});
        case 17:
            return Opcode(::irgen::LTE{});
        case 18:
            return Opcode(::irgen::GT{});
        case 19:
            return Opcode(::irgen::GTE{});
        case 20:
            return Opcode(::irgen::STORE{});
        case 21: {
            ::irgen::LOAD op("");
            op.var_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 22:
            return Opcode(::irgen::LOAD_FAST(static_cast<size_t>(r.read_u64())));
        case 23:
            return Opcode(::irgen::STORE_FAST(static_cast<size_t>(r.read_u64())));
        case 24: {
            const size_t slot = static_cast<size_t>(r.read_u64());
            const size_t var_id = static_cast<size_t>(r.read_u64());
            ::irgen::BIND_FAST op(slot, "");
            op.var_id = var_id;
            return Opcode(std::move(op));
        }
        case 25:
            return Opcode(::irgen::LABEL(static_cast<size_t>(r.read_u64())));
        case 26:
            return Opcode(::irgen::GOTO(static_cast<size_t>(r.read_u64())));
        case 27:
            return Opcode(::irgen::GOTOIF(static_cast<size_t>(r.read_u64())));
        case 28:
            return Opcode(::irgen::GOTOIFNOT(static_cast<size_t>(r.read_u64())));
        case 29:
            return Opcode(::irgen::ENTER_SCOPE{});
        case 30:
            return Opcode(::irgen::LEAVE_SCOPE{});
        case 31: {
            const size_t argc = static_cast<size_t>(r.read_u64());
            const bool kwargs = r.read_bool();
            const uint64_t mask = r.read_u64();
            return Opcode(::irgen::CALL(argc, kwargs, mask));
        }
        case 32:
            return Opcode(::irgen::RET{});
        case 33: {
            ::irgen::FINDMOD op("");
            op.module_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 34: {
            ::irgen::GETATTR op("");
            op.name_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 35:
            return Opcode(::irgen::VEC_NEW(static_cast<size_t>(r.read_u64())));
        case 36:
            return Opcode(::irgen::DICT_NEW(static_cast<size_t>(r.read_u64())));
        case 37:
            return Opcode(::irgen::INDEX{});
        case 38: {
            ::irgen::STORE_ARG op("");
            op.var_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 39: {
            ::irgen::NEW_VAR op("");
            op.var_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 40: {
            ::irgen::NEW_CONST op("");
            op.var_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 41: {
            ::irgen::NEW_INTERN_VAR op("");
            op.var_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 42: {
            ::irgen::NEW_INTERN_CONST op("");
            op.var_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 43: {
            ::irgen::NEW_VAR_OR_LOAD op("");
            op.var_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 44:
            return Opcode(::irgen::RET_THEN_LEAVE_SCOPE{});
        case 45:
            return Opcode(::irgen::ITER_NEW{});
        case 46:
            return Opcode(::irgen::ITER_NEXT{});
        case 47:
            return Opcode(::irgen::ITER_END{});
        case 48:
            return Opcode(::irgen::THROW{});
        case 49: {
            const size_t catch_l = static_cast<size_t>(r.read_u64());
            const size_t else_l = static_cast<size_t>(r.read_u64());
            const size_t end_l = static_cast<size_t>(r.read_u64());
            return Opcode(::irgen::ENTER_TRY(catch_l, else_l, end_l));
        }
        case 50:
            return Opcode(::irgen::END_TRY{});
        case 51:
            return Opcode(::irgen::POP_TRY{});
        case 52:
            return Opcode(::irgen::PUSH_EXC{});
        case 53: {
            ::irgen::EXC_MATCH op("");
            op.type_name_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 54: {
            ::irgen::IS_INSTANCE op("");
            op.type_name_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 55:
            return Opcode(::irgen::RETHROW{});
        case 56: {
            ::irgen::STRUCT_NEW op("", 0);
            op.struct_id = static_cast<size_t>(r.read_u64());
            op.arg_count = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 57: {
            ::irgen::SET_FIELD op("");
            op.name_id = static_cast<size_t>(r.read_u64());
            return Opcode(std::move(op));
        }
        case 58:
            return Opcode(::irgen::IS_VECTOR{});
        case 59:
            return Opcode(::irgen::VEC_LEN{});
        case 60:
            return Opcode(::irgen::MATCH_EQ{});
        default:
            throw std::runtime_error("LMC: unknown opcode tag: " + std::to_string(tag));
    }
}

std::vector<Opcode> read_opcodes(ByteReader& r) {
    const uint64_t count = r.read_u64();
    std::vector<Opcode> code;
    code.reserve(static_cast<size_t>(count));
    for (uint64_t i = 0; i < count; ++i) {
        code.push_back(read_opcode(r));
    }
    return code;
}

Value read_value(ByteReader& r) {
    const auto tag = static_cast<ValueTag>(r.read_u8());
    switch (tag) {
        case ValueTag::None:
            return Value();
        case ValueTag::Number:
            return Value(lang::lammp::Number::fromString(r.read_string()));
        case ValueTag::Bool:
            return Value(r.read_bool());
        case ValueTag::String:
            return Value(r.read_string());
        case ValueTag::Rational: {
            const auto num = lang::lammp::Number::fromString(r.read_string());
            const auto den = lang::lammp::Number::fromString(r.read_string());
            return Value(lang::lammp::Rational(num, den));
        }
        case ValueTag::Vector: {
            const uint64_t count = r.read_u64();
            std::vector<std::shared_ptr<Value>> vec;
            vec.reserve(static_cast<size_t>(count));
            for (uint64_t i = 0; i < count; ++i) {
                vec.push_back(std::make_shared<Value>(read_value(r)));
            }
            return Value(std::move(vec));
        }
        case ValueTag::UserFunction:
            return Value(read_function(r));
        case ValueTag::TypeHandle:
            return ::irgen::make_type_value(r.read_string());
        case ValueTag::FriendFunction:
            return Value(read_friend_function(r));
        case ValueTag::RuntimeAst:
            return ::irgen::make_ast_value(read_runtime_ast(r));
        default:
            throw std::runtime_error("LMC: unknown value tag");
    }
}

std::shared_ptr<FunctionObject> read_function(ByteReader& r) {
    auto func = std::make_shared<FunctionObject>();
    func->name = r.read_string();
    const uint64_t param_count = r.read_u64();
    func->params.reserve(static_cast<size_t>(param_count));
    for (uint64_t i = 0; i < param_count; ++i) {
        func->params.push_back(r.read_string());
    }
    const uint64_t type_count = r.read_u64();
    func->param_types.reserve(static_cast<size_t>(type_count));
    for (uint64_t i = 0; i < type_count; ++i) {
        if (r.read_bool()) {
            func->param_types.emplace_back(r.read_string());
        } else {
            func->param_types.emplace_back();
        }
    }
    const uint64_t default_count = r.read_u64();
    func->param_default_ir.resize(static_cast<size_t>(default_count));
    for (uint64_t i = 0; i < default_count; ++i) {
        func->param_default_ir[static_cast<size_t>(i)] = read_opcodes(r);
    }
    func->body = read_opcodes(r);
    func->location = static_cast<size_t>(r.read_u64());
    func->needs_closure = r.read_bool();
    func->needs_symbol_bind = r.read_bool();
    func->is_macro = r.read_bool();
    if (r.read_bool()) {
        func->variadic_param_index = static_cast<size_t>(r.read_u64());
    }
    return func;
}

std::shared_ptr<FriendFunctionObject> read_friend_function(ByteReader& r) {
    auto obj = ::irgen::make_friend_function(r.read_string());
    const uint64_t count = r.read_u64();
    for (uint64_t i = 0; i < count; ++i) {
        obj->dispatch_handlers().push_back(std::make_shared<Value>(Value(read_function(r))));
    }
    return obj;
}

RuntimeAstNode read_runtime_ast(ByteReader& r) {
    RuntimeAstNode node;
    node.kind = static_cast<lmx::ASTNodeType>(r.read_u32());
    node.line = static_cast<int>(r.read_u32());
    node.text = r.read_string();
    node.bool_val = r.read_bool();

    const uint64_t stmt_count = r.read_u64();
    node.stmts.reserve(static_cast<size_t>(stmt_count));
    for (uint64_t i = 0; i < stmt_count; ++i) {
        node.stmts.push_back(read_runtime_ast(r));
    }
    const uint64_t child_count = r.read_u64();
    node.children.reserve(static_cast<size_t>(child_count));
    for (uint64_t i = 0; i < child_count; ++i) {
        node.children.push_back(read_runtime_ast(r));
    }

    const auto read_optional = [&]() -> std::unique_ptr<RuntimeAstNode> {
        if (!r.read_bool()) {
            return nullptr;
        }
        return std::make_unique<RuntimeAstNode>(read_runtime_ast(r));
    };
    node.slot_a = read_optional();
    node.slot_b = read_optional();
    node.slot_c = read_optional();

    const uint64_t hygienic_count = r.read_u64();
    for (uint64_t i = 0; i < hygienic_count; ++i) {
        node.hygienic_names.push_back(r.read_string());
    }
    const uint64_t binding_name_count = r.read_u64();
    for (uint64_t i = 0; i < binding_name_count; ++i) {
        node.binding_names.push_back(r.read_string());
    }
    const uint64_t binding_count = r.read_u64();
    for (uint64_t i = 0; i < binding_count; ++i) {
        node.bindings.push_back(read_runtime_ast(r));
    }

    const uint64_t arg_count = r.read_u64();
    for (uint64_t i = 0; i < arg_count; ++i) {
        RuntimeAstNode::CallArg arg;
        arg.kw_name = r.read_string();
        arg.is_splat = r.read_bool();
        if (r.read_bool()) {
            arg.value = std::make_unique<RuntimeAstNode>(read_runtime_ast(r));
        }
        node.call_args.push_back(std::move(arg));
    }
    return node;
}

StructFieldDef read_struct_field(ByteReader& r) {
    StructFieldDef field;
    field.name = r.read_string();
    field.type_name = r.read_string();
    field.has_type_annotation = r.read_bool();
    field.mutable_field = r.read_bool();
    field.has_default = r.read_bool();
    if (field.has_default) {
        field.default_value = read_value(r);
    }
    return field;
}

StructTypeDef read_type_def(ByteReader& r) {
    StructTypeDef def;
    def.name = r.read_string();
    def.base_name = r.read_string();
    def.kind = static_cast<::irgen::TypeKind>(r.read_u8());
    def.typed = r.read_bool();
    const uint64_t field_count = r.read_u64();
    for (uint64_t i = 0; i < field_count; ++i) {
        def.fields.push_back(read_struct_field(r));
    }
    const uint64_t method_count = r.read_u64();
    for (uint64_t i = 0; i < method_count; ++i) {
        const std::string method_name = r.read_string();
        def.methods[method_name] = read_function(r);
    }
    if (r.read_bool()) {
        def.convert_func = read_friend_function(r);
    }
    return def;
}

} // namespace

OptimizeReport optimize_bytecode_deep(std::vector<Opcode>& code) {
    OptimizeReport report = optimize_bytecode(code);
    for (auto& op : code) {
        if (auto* push = std::get_if<::irgen::PUSH>(&op)) {
            optimize_value_tree(push->val);
        }
    }
    return report;
}

CompiledModule compile_ast(const lmx::ProgramASTNode* program) {
    if (!program) {
        throw std::runtime_error("compile_ast: null program");
    }

    CompiledModule module;
    module.source_filename = program->source_filename.empty() ? "<input>" : program->source_filename;
    const bool prev_opt = bytecode_optimize_enabled;
    bytecode_optimize_enabled = false;
    module.code = Generator(const_cast<lmx::ProgramASTNode*>(program)).gen();
    bytecode_optimize_enabled = prev_opt;
    Generator::replace_string(module.code);
    module.string_pool = ::irgen::g_string_pool.export_strings();
    module.type_defs = collect_user_type_defs(module.code);
    return module;
}

CompiledModule compile_ast_optimized(const lmx::ProgramASTNode* program) {
    CompiledModule module = compile_ast(program);
    module.optimize_report = optimize_bytecode_deep(module.code);
    module.optimized = true;
    return module;
}

void save_lmc(const std::string& path, const CompiledModule& module) {
    ByteWriter w;
    w.write_bytes(LMC_MAGIC, 4);
    w.write_u32(LMC_FORMAT_VERSION);
    w.write_u32(module.optimized ? LMC_FLAG_OPTIMIZED : 0u);
    w.write_string(module.source_filename);

    w.write_u64(module.string_pool.size());
    for (const auto& s : module.string_pool) {
        w.write_string(s);
    }

    w.write_u64(module.type_defs.size());
    for (const auto& def : module.type_defs) {
        write_type_def(w, def);
    }

    write_opcodes(w, module.code);

    const std::vector<uint8_t>& bytes = w.data();
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot open for write: " + path);
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        throw std::runtime_error("failed to write: " + path);
    }
}

CompiledModule load_lmc(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open for read: " + path);
    }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12) {
        throw std::runtime_error("LMC: file too small");
    }

    ByteReader r(bytes);
    char magic[4]{};
    r.read_bytes(magic, 4);
    if (std::memcmp(magic, LMC_MAGIC, 4) != 0) {
        throw std::runtime_error("LMC: invalid magic");
    }
    const uint32_t version = r.read_u32();
    if (version != LMC_FORMAT_VERSION) {
        throw std::runtime_error("LMC: unsupported format version");
    }
    const uint32_t flags = r.read_u32();

    CompiledModule module;
    module.optimized = (flags & LMC_FLAG_OPTIMIZED) != 0;
    module.source_filename = r.read_string();

    const uint64_t pool_count = r.read_u64();
    module.string_pool.reserve(static_cast<size_t>(pool_count));
    for (uint64_t i = 0; i < pool_count; ++i) {
        module.string_pool.push_back(r.read_string());
    }
    ::irgen::g_string_pool.rebuild(module.string_pool);

    const uint64_t type_count = r.read_u64();
    for (uint64_t i = 0; i < type_count; ++i) {
        module.type_defs.push_back(read_type_def(r));
    }

    for (auto& def : module.type_defs) {
        (void)::irgen::register_type_def(std::move(def));
    }

    module.code = read_opcodes(r);
    if (r.remaining() != 0) {
        throw std::runtime_error("LMC: trailing bytes in file");
    }
    return module;
}

bool run_compiled_module(
    CompiledModule& module,
    const std::function<bool(::irgen::VM& vm)>& on_result
) {
    auto vm = std::make_unique<::irgen::VM>(std::move(module.code));
    vm->source_filename = module.source_filename;
    vm->set_symbol("__package__", ::irgen::Value("__main__"));
    vm->run();

    if (!on_result) {
        vm->shutdown();
        return true;
    }
    const bool ok = on_result(*vm);
    vm->shutdown();
    return ok;
}

} // namespace lm::irgen
