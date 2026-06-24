#include "optimizer.hpp"

#include "friend_function.hpp"

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace lm::irgen {
namespace {

using ::irgen::ADD;
using ::irgen::DIV;
using ::irgen::ENTER_TRY;
using ::irgen::GOTO;
using ::irgen::GOTOIF;
using ::irgen::GOTOIFNOT;
using ::irgen::LABEL;
using ::irgen::MUL;
using ::irgen::Opcode;
using ::irgen::PUSH;
using ::irgen::SUB;
using ::irgen::Value;

[[nodiscard]] std::optional<lang::lammp::Number> push_number(const Opcode& op) {
    const auto* push = std::get_if<PUSH>(&op);
    if (push == nullptr) {
        return std::nullopt;
    }
    const Value& val = push->val;
    if (!val.isNumber()) {
        return std::nullopt;
    }
    return val.asNumber();
}

[[nodiscard]] std::unordered_map<size_t, size_t> build_label_table(const std::vector<Opcode>& code) {
    std::unordered_map<size_t, size_t> table;
    for (size_t i = 0; i < code.size(); ++i) {
        if (const auto* label = std::get_if<LABEL>(&code[i])) {
            table[label->label_id] = i;
        }
    }
    return table;
}

[[nodiscard]] bool is_control_flow_opcode(const Opcode& op) {
    return std::holds_alternative<LABEL>(op) ||
           std::holds_alternative<GOTO>(op) ||
           std::holds_alternative<GOTOIF>(op) ||
           std::holds_alternative<GOTOIFNOT>(op);
}

void collect_implicit_jump_targets_from_opcodes(
    const std::vector<Opcode>& code,
    std::unordered_set<size_t>& jump_targets
);

void collect_implicit_jump_targets_from_value(
    const Value& val,
    std::unordered_set<size_t>& jump_targets
) {
    if (val.isUserFunction()) {
        const auto& func = val.asFunctionObject();
        jump_targets.insert(func->location);
        for (const auto& ir : func->param_default_ir) {
            collect_implicit_jump_targets_from_opcodes(ir, jump_targets);
        }
        collect_implicit_jump_targets_from_opcodes(func->body, jump_targets);
        return;
    }
    if (val.isFriendFunction()) {
        for (const auto& handler : val.asFriendFunction()->dispatch_handlers()) {
            if (handler) {
                collect_implicit_jump_targets_from_value(*handler, jump_targets);
            }
        }
        return;
    }
    if (val.isTypeHandle()) {
        const auto& def = val.asTypeDef();
        for (const auto& [_, method] : def->methods) {
            if (method) {
                jump_targets.insert(method->location);
                for (const auto& ir : method->param_default_ir) {
                    collect_implicit_jump_targets_from_opcodes(ir, jump_targets);
                }
                collect_implicit_jump_targets_from_opcodes(method->body, jump_targets);
            }
        }
        return;
    }
    if (val.isVector()) {
        for (const auto& elem : val.asVector()) {
            if (elem) {
                collect_implicit_jump_targets_from_value(*elem, jump_targets);
            }
        }
    }
}

void collect_implicit_jump_targets_from_opcodes(
    const std::vector<Opcode>& code,
    std::unordered_set<size_t>& jump_targets
) {
    for (const auto& op : code) {
        if (const auto* push = std::get_if<PUSH>(&op)) {
            collect_implicit_jump_targets_from_value(push->val, jump_targets);
        } else if (const auto* try_op = std::get_if<ENTER_TRY>(&op)) {
            jump_targets.insert(try_op->catch_label);
            jump_targets.insert(try_op->else_label);
            jump_targets.insert(try_op->end_label);
        }
    }
}

[[nodiscard]] std::unordered_set<size_t> collect_jump_targets(const std::vector<Opcode>& code) {
    std::unordered_set<size_t> jump_targets;
    for (const auto& op : code) {
        if (const auto* jump = std::get_if<GOTO>(&op)) {
            jump_targets.insert(jump->label_id);
        } else if (const auto* jump = std::get_if<GOTOIF>(&op)) {
            jump_targets.insert(jump->label_id);
        } else if (const auto* jump = std::get_if<GOTOIFNOT>(&op)) {
            jump_targets.insert(jump->label_id);
        }
    }
    collect_implicit_jump_targets_from_opcodes(code, jump_targets);
    return jump_targets;
}

size_t fold_constants(std::vector<Opcode>& code) {
    size_t folds = 0;

    for (size_t i = 0; i + 2 < code.size(); ++i) {
        const auto left = push_number(code[i]);
        const auto right = push_number(code[i + 1]);
        if (!left || !right) {
            continue;
        }

        std::optional<lang::lammp::Number> folded;
        if (std::holds_alternative<ADD>(code[i + 2])) {
            folded = *left + *right;
        } else if (std::holds_alternative<SUB>(code[i + 2])) {
            folded = *left - *right;
        } else if (std::holds_alternative<MUL>(code[i + 2])) {
            folded = *left * *right;
        } else if (std::holds_alternative<DIV>(code[i + 2])) {
            if (right->isZero()) {
                continue;
            }
            folded = *left / *right;
        }

        if (!folded) {
            continue;
        }

        code[i] = PUSH(Value(*folded));
        code.erase(code.begin() + static_cast<std::ptrdiff_t>(i + 1),
                   code.begin() + static_cast<std::ptrdiff_t>(i + 3));
        ++folds;
        if (i > 0) {
            --i;
        }
    }

    return folds;
}

[[nodiscard]] size_t resolve_goto_target(
    size_t label_id,
    const std::unordered_map<size_t, size_t>& label_table,
    const std::vector<Opcode>& code
) {
    std::unordered_set<size_t> visited;
    while (visited.insert(label_id).second) {
        const auto it = label_table.find(label_id);
        if (it == label_table.end()) {
            break;
        }
        const size_t pc = it->second;
        if (pc + 1 >= code.size()) {
            break;
        }
        const auto* next_goto = std::get_if<GOTO>(&code[pc + 1]);
        if (next_goto == nullptr) {
            break;
        }
        label_id = next_goto->label_id;
    }
    return label_id;
}

size_t thread_jumps(std::vector<Opcode>& code) {
    const auto label_table = build_label_table(code);
    size_t threaded = 0;

    for (auto& op : code) {
        if (auto* jump = std::get_if<GOTO>(&op)) {
            const size_t resolved = resolve_goto_target(jump->label_id, label_table, code);
            if (resolved != jump->label_id) {
                jump->label_id = resolved;
                ++threaded;
            }
        } else if (auto* jump = std::get_if<GOTOIF>(&op)) {
            const size_t resolved = resolve_goto_target(jump->label_id, label_table, code);
            if (resolved != jump->label_id) {
                jump->label_id = resolved;
                ++threaded;
            }
        } else if (auto* jump = std::get_if<GOTOIFNOT>(&op)) {
            const size_t resolved = resolve_goto_target(jump->label_id, label_table, code);
            if (resolved != jump->label_id) {
                jump->label_id = resolved;
                ++threaded;
            }
        }
    }

    return threaded;
}

size_t remove_dead_after_goto(std::vector<Opcode>& code) {
    const auto label_table = build_label_table(code);
    const auto jump_targets = collect_jump_targets(code);

    std::vector<bool> keep(code.size(), true);
    size_t removed = 0;

    for (size_t i = 0; i < code.size(); ++i) {
        const auto* jump = std::get_if<GOTO>(&code[i]);
        if (jump == nullptr) {
            continue;
        }
        const auto target_it = label_table.find(jump->label_id);
        if (target_it == label_table.end()) {
            continue;
        }
        const size_t target_pc = target_it->second;
        if (target_pc <= i + 1) {
            continue;
        }

        bool blocked = false;
        for (size_t j = i + 1; j < target_pc; ++j) {
            if (const auto* label = std::get_if<LABEL>(&code[j])) {
                if (jump_targets.contains(label->label_id)) {
                    blocked = true;
                    break;
                }
            }
        }
        if (blocked) {
            continue;
        }

        for (size_t j = i + 1; j < target_pc; ++j) {
            if (keep[j] && !is_control_flow_opcode(code[j])) {
                keep[j] = false;
                ++removed;
            }
        }
    }

    if (removed == 0) {
        return 0;
    }

    std::vector<Opcode> next;
    next.reserve(code.size() - removed);
    for (size_t i = 0; i < code.size(); ++i) {
        if (keep[i]) {
            next.push_back(std::move(code[i]));
        }
    }
    code = std::move(next);
    return removed;
}

} // namespace

OptimizeReport optimize_bytecode(std::vector<Opcode>& code) {
    OptimizeReport report;
    report.ops_before = code.size();

    report.constant_folds += fold_constants(code);
    report.jumps_threaded += thread_jumps(code);
    report.dead_ops_removed += remove_dead_after_goto(code);
    report.constant_folds += fold_constants(code);

    report.ops_after = code.size();
    return report;
}

void maybe_optimize_bytecode(std::vector<Opcode>& code) {
    if (bytecode_optimize_enabled) {
        (void)optimize_bytecode(code);
    }
}

} // namespace lm::irgen
