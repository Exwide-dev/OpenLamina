#include "exceptions.hpp"

#include "struct_types.hpp"

#include <cstring>

namespace irgen {
namespace {

void add_message_field(StructTypeDef& def) {
    StructFieldDef fd;
    fd.name = "message";
    fd.type_name = "text";
    fd.has_type_annotation = true;
    fd.mutable_field = true;
    fd.has_default = true;
    fd.default_value = Value("");
    def.fields.push_back(fd);
}

void register_exception_type(const char* name, const char* base) {
    StructTypeDef def;
    def.name = name;
    def.kind = TypeKind::User;
    if (base != nullptr && base[0] != '\0') {
        def.base_name = base;
        def.fields = get_type_def(base)->fields;
    } else {
        add_message_field(def);
    }
    (void)register_type_def(std::move(def));
}

[[nodiscard]] std::string vm_current_scope(const VM& vm) {
    if (vm.call_func_stack.empty()) {
        return "global-scope";
    }
    return vm.call_func_stack.back()->name;
}

[[nodiscard]] size_t call_site_pc(const VM& vm, const size_t frame_index) {
    if (frame_index >= vm.call_stack.size()) {
        return SIZE_MAX;
    }
    return vm.call_stack[frame_index];
}

} // namespace

TraceFrame vm_exception_site(const VM& vm) {
    TraceFrame frame;
    frame.filename = vm.source_filename;
    frame.column = 0;
    frame.scope = vm_current_scope(vm);
    if (vm.pc < vm.code.size()) {
        frame.line = get_opcode_line_no(vm.code[vm.pc]);
        frame.source_line = get_opcode_line(vm.code[vm.pc]);
    }
    return frame;
}

std::vector<TraceFrame> build_traceback(const VM& vm) {
    std::vector<TraceFrame> frames;
    frames.reserve(vm.call_func_stack.size());

    for (size_t i = 0; i < vm.call_func_stack.size(); ++i) {
        TraceFrame frame;
        frame.filename = vm.source_filename;
        frame.scope = vm.call_func_stack[i]->name;
        const size_t site_pc = call_site_pc(vm, i);
        if (site_pc != SIZE_MAX && site_pc < vm.code.size()) {
            frame.line = get_opcode_line_no(vm.code[site_pc]);
            frame.source_line = get_opcode_line(vm.code[site_pc]);
        }
        frames.push_back(std::move(frame));
    }

    return frames;
}

void unwind_to_try_frame(VM& vm, const TryHandlerFrame& frame) {
    vm.op_stack.clear();

    while (vm.call_func_stack.size() > frame.call_func_stack_sz) {
        if (!vm.call_func_stack.empty()) {
            const auto& func = vm.call_func_stack.back();
            if (!func->closure.empty()) {
                for (size_t i = 0; i < func->closure.size(); ++i) {
                    if (!vm.symbol_stack.empty()) {
                        vm.symbol_stack.pop_back();
                    }
                }
            }
            vm.call_func_stack.pop_back();
        }
        if (!vm.call_stack.empty()) {
            vm.call_stack.pop_back();
        }
    }

    while (vm.symbol_stack.size() > frame.symbol_stack_sz) {
        vm.symbol_stack.pop_back();
    }
    while (vm.locals_stack.size() > frame.locals_stack_sz) {
        vm.locals_stack.pop_back();
    }
}

void register_builtin_exceptions() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    register_exception_type("BaseException", nullptr);
    register_exception_type("Exception", "BaseException");
    register_exception_type("StopIteration", "Exception");
    register_exception_type("RuntimeError", "Exception");
    register_exception_type("ValueError", "Exception");
    register_exception_type("TypeError", "Exception");
}

std::string exception_type_name(const Value& exc) {
    const Value& obj = exc.deref();
    if (obj.isStruct()) {
        return obj.asStruct()->type->name;
    }
    return obj.type_name();
}

std::string exception_message(const Value& exc) {
    const Value& obj = exc.deref();
    if (!obj.isStruct()) {
        return obj.toString();
    }

    const auto inst = obj.asStruct();
    for (size_t i = 0; i < inst->type->fields.size(); ++i) {
        if (inst->type->fields[i].name == "message") {
            const Value& msg = inst->slots[i];
            if (msg.isString()) {
                return msg.asString();
            }
            return msg.toString();
        }
    }
    return inst->type->name;
}

Value make_exception_instance(VM& vm, const char* type_name, const std::string& message) {
    std::vector<Value> positional;
    if (!message.empty()) {
        positional.emplace_back(message);
    }
    return make_struct_instance(vm, type_name, std::move(positional));
}

bool value_is_exception(const Value& value) {
    const Value& obj = value.deref();
    if (!obj.isStruct()) {
        return false;
    }
    return struct_instance_is_a(value, "BaseException");
}

bool dispatch_exception(
    VM& vm,
    const Value& exc,
    TraceFrame site,
    std::vector<TraceFrame> traceback
) {
    (void)traceback;
    if (vm.try_stack.empty()) {
        return false;
    }

    const TryHandlerFrame frame = vm.try_stack.back();
    unwind_to_try_frame(vm, frame);

    vm.active_exception = vm.cell_pool.allocateCopy(exc.deref());

    const auto label_it = vm.label_table.find(frame.catch_label);
    if (label_it == vm.label_table.end()) {
        raise_cpp_runtime_error(vm, "internal error: try catch label not found");
    }
    vm.pc = label_it->second;

    if (site.line <= 0) {
        site = vm_exception_site(vm);
    }
    (void)site;
    return true;
}

bool dispatch_runtime_error(VM& vm, const RuntimeError& err) {
    if (vm.try_stack.empty()) {
        return false;
    }

    TraceFrame site = err.error_site();
    std::vector<TraceFrame> traceback = err.traceback();
    if (site.line <= 0 && traceback.empty()) {
        site = vm_exception_site(vm);
        traceback = build_traceback(vm);
    }

    Value exc;
    if (!err.exception_type().empty()) {
        exc = make_exception_instance(vm, err.exception_type().c_str(), err.what());
    } else {
        exc = make_exception_instance(vm, "RuntimeError", err.what());
    }

    if (struct_instance_is_a(exc, "StopIteration")) {
        return false;
    }

    return dispatch_exception(vm, exc, site, std::move(traceback));
}

void raise_cpp_runtime_error(
    VM& vm,
    std::string msg,
    std::string type_name,
    const bool stop_iteration
) {
    RuntimeError err(std::move(msg), vm_exception_site(vm), build_traceback(vm));
    if (!type_name.empty()) {
        err.set_exception_type(std::move(type_name));
    }
    if (stop_iteration) {
        err.mark_stop_iteration();
    }
    throw err;
}

void throw_user_exception(VM& vm, const Value& exc) {
    if (!value_is_exception(exc)) {
        raise_cpp_runtime_error(
            vm,
            std::format("can only throw exception objects, got {}", exc.deref().type_name())
        );
    }

    if (struct_instance_is_a(exc, "StopIteration") && vm.iter_next_guard_depth > 0) {
        raise_cpp_runtime_error(vm, "StopIteration", "StopIteration", true);
    }

    if (dispatch_exception(vm, exc, vm_exception_site(vm), build_traceback(vm))) {
        return;
    }

    const std::string type_name = exception_type_name(exc);
    std::string msg = exception_message(exc);
    if (msg.empty()) {
        msg = type_name;
    }
    raise_cpp_runtime_error(
        vm,
        std::move(msg),
        type_name,
        struct_instance_is_a(exc, "StopIteration")
    );
}

void raise_stop_iteration(VM& vm) {
    raise_cpp_runtime_error(vm, "StopIteration", "StopIteration", true);
}

bool is_stop_iteration(const RuntimeError& error) {
    if (error.is_stop_iteration()) {
        return true;
    }
    if (error.exception_type() == "StopIteration") {
        return true;
    }
    return std::strcmp(error.what(), "StopIteration") == 0;
}

} // namespace irgen
