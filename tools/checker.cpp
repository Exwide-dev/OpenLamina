#include "checker.hpp"
#include "tools/lang/number.hpp"
#include "front-end/front_end.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <unordered_set>

namespace olmcheck {

// ===========================================================================
// Verbose logging
// ===========================================================================
namespace {
bool g_view_log_enabled = false;
}

void view_log(const std::string& msg) {
    if (g_view_log_enabled) {
        std::cout << LOG_CYAN << "[LOG] " << LOG_RESET << msg << "\n";
    }
}

void view_log_info(const std::string& msg) {
    if (g_view_log_enabled) {
        std::cout << LOG_DIM << "[INFO] " << LOG_RESET << msg << "\n";
    }
}

void view_log_match(const std::string& context, const TypePtr& t1,
                    const TypePtr& t2, int line) {
    if (g_view_log_enabled) {
        std::cout << LOG_GREEN << "[MATCH] " << LOG_RESET << context
                  << ": " << LOG_BLUE << t1->to_string() << LOG_RESET
                  << " ~ " << LOG_BLUE << t2->to_string() << LOG_RESET
                  << " (line " << line << ")\n";
    }
}

} // namespace olmcheck

// ===========================================================================
// Anonymous helpers: aliases, builtins, Levenshtein
// ===========================================================================
namespace {

const std::unordered_map<std::string, std::string>& alias_map() {
    static const std::unordered_map<std::string, std::string> m = {
        {"int", "num"}, {"integer", "num"}, {"float", "num"},
        {"double", "num"}, {"real", "num"}, {"number", "num"},
        {"string", "text"}, {"str", "text"},
        {"bool", "bool"}, {"boolean", "bool"},
        {"list", "vector"}, {"vec", "vector"},
        {"dict", "table"}, {"map", "table"}, {"object", "table"},
        {"none", "nonetype"}, {"null", "nonetype"}, {"nothing", "nonetype"}, {"nil", "nonetype"},
        {"any", "any"},
        {"Maybe", "Maybe"}, {"Optional", "Maybe"},
        {"Union", "Union"},
        {"Covariant", "Covariant"}, {"Contravariant", "Contravariant"}, {"Invariant", "Invariant"},
    };
    return m;
}

const std::unordered_set<std::string>& primitive_builtins() {
    static const std::unordered_set<std::string> s = {
        "num", "text", "bool", "vector", "table", "nonetype", "any"
    };
    return s;
}

const std::unordered_set<std::string>& composite_builtins() {
    static const std::unordered_set<std::string> s = {
        "Maybe", "Union", "Covariant", "Contravariant", "Invariant", "vector"
    };
    return s;
}

bool is_numeric_name(const std::string& n) {
    return n == "num" || n == "int" || n == "integer" || n == "float" ||
           n == "double" || n == "real";
}

int levenshtein(const std::string& a, const std::string& b) {
    const std::size_t m = a.size();
    const std::size_t n = b.size();
    std::vector<int> prev(n + 1), cur(n + 1);
    for (std::size_t j = 0; j <= n; ++j) prev[j] = static_cast<int>(j);
    for (std::size_t i = 1; i <= m; ++i) {
        cur[0] = static_cast<int>(i);
        for (std::size_t j = 1; j <= n; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, cur);
    }
    return prev[n];
}

/// Recursively collect every struct / function declaration reachable from node.
void collect_symbols(const lmx::ASTNode* node,
                     std::unordered_map<std::string, const lmx::StructDeclNode*>& structs,
                     std::unordered_map<std::string, const lmx::FuncDeclNode*>& funcs) {
    if (!node) return;

    if (const auto* prog = dynamic_cast<const lmx::ProgramASTNode*>(node)) {
        for (const auto* s : prog->stmts) collect_symbols(s, structs, funcs);
    } else if (const auto* s = dynamic_cast<const lmx::StructDeclNode*>(node)) {
        structs[s->name] = s;
        for (const auto* m : s->methods) collect_symbols(m, structs, funcs);
    } else if (const auto* f = dynamic_cast<const lmx::FuncDeclNode*>(node)) {
        funcs[f->name] = f;
        if (f->body) for (const auto* s : f->body->stmts) collect_symbols(s, structs, funcs);
    } else if (const auto* b = dynamic_cast<const lmx::BlockStmtNode*>(node)) {
        for (const auto* s : b->stmts) collect_symbols(s, structs, funcs);
    } else if (const auto* mod = dynamic_cast<const lmx::ModuleNode*>(node)) {
        for (const auto* v : mod->vars) collect_symbols(v, structs, funcs);
        for (const auto* f : mod->ord_funcs) collect_symbols(f, structs, funcs);
        for (const auto* c : mod->children) collect_symbols(c, structs, funcs);
    } else if (const auto* ifs = dynamic_cast<const lmx::IfStmtNode*>(node)) {
        if (ifs->then_block) collect_symbols(ifs->then_block, structs, funcs);
        for (const auto& e : ifs->elif_blocks) if (e.block) collect_symbols(e.block, structs, funcs);
        if (ifs->else_block) collect_symbols(ifs->else_block, structs, funcs);
    } else if (const auto* loop = dynamic_cast<const lmx::ForLoopNode*>(node)) {
        if (loop->body) collect_symbols(loop->body, structs, funcs);
    } else if (const auto* wh = dynamic_cast<const lmx::WhileStmtNode*>(node)) {
        if (wh->body) collect_symbols(wh->body, structs, funcs);
    } else if (const auto* lp = dynamic_cast<const lmx::LoopNode*>(node)) {
        if (lp->body) collect_symbols(lp->body, structs, funcs);
    } else if (const auto* dec = dynamic_cast<const lmx::DecoratedFuncNode*>(node)) {
        collect_symbols(dec->target, structs, funcs);
    }
}

} // anonymous namespace

// ===========================================================================
// SourceManager
// ===========================================================================
namespace olmcheck {

const std::string* SourceManager::line_text(int line) const {
    if (line <= 0) return nullptr;
    if (static_cast<std::size_t>(line) > lines_.size()) return nullptr;
    return &lines_[static_cast<std::size_t>(line) - 1];
}

// ===========================================================================
// Reporter
// ===========================================================================
size_t Reporter::error_count() const {
    size_t c = 0;
    for (const auto& i : issues_) if (i.severity == Issue::Severity::Error) ++c;
    return c;
}

size_t Reporter::warning_count() const {
    size_t c = 0;
    for (const auto& i : issues_) if (i.severity == Issue::Severity::Warning) ++c;
    return c;
}

void Reporter::emit(const Issue& issue) {
    issues_.push_back(issue);
    render(issue);
}

void Reporter::error(const SourceLoc& loc, const std::string& msg,
                     std::string code, std::string suggestion) {
    Issue i{Issue::Severity::Error, loc, std::move(code), msg, std::move(suggestion), {}};
    emit(i);
}

void Reporter::warning(const SourceLoc& loc, const std::string& msg,
                       std::string code, std::string suggestion) {
    Issue i{Issue::Severity::Warning, loc, std::move(code), msg, std::move(suggestion), {}};
    emit(i);
}

void Reporter::note(const SourceLoc& loc, const std::string& msg) {
    Issue i{Issue::Severity::Note, loc, {}, msg, {}, {}};
    emit(i);
}

void Reporter::hint(const SourceLoc& loc, const std::string& msg) {
    Issue i{Issue::Severity::Hint, loc, {}, msg, {}, {}};
    emit(i);
}

void Reporter::error(int line, const std::string& msg, const std::string& file) {
    SourceLoc loc; loc.line = line; loc.file = file;
    Issue i{Issue::Severity::Error, loc, {}, msg, {}, {}};
    emit(i);
}

void Reporter::render(const Issue& issue) const {
    auto& out = std::cerr;
    const char* sev_color = LOG_RED;
    const char* sev_label = "error";
    switch (issue.severity) {
        case Issue::Severity::Warning: sev_color = LOG_YELLOW; sev_label = "warning"; break;
        case Issue::Severity::Note:    sev_color = LOG_CYAN;    sev_label = "note";    break;
        case Issue::Severity::Hint:    sev_color = LOG_MAGENTA; sev_label = "hint";    break;
        case Issue::Severity::Error:   sev_color = LOG_RED;     sev_label = "error";   break;
    }

    const std::string& file = issue.loc.file.empty() && src_ ? src_->file() : issue.loc.file;

    if (color_) {
        out << sev_color << LOG_BOLD << sev_label << LOG_RESET;
        if (!issue.code.empty()) out << LOG_DIM << "[" << issue.code << "]" << LOG_RESET;
        out << ": " << issue.message << "\n";
    } else {
        out << sev_label;
        if (!issue.code.empty()) out << "[" << issue.code << "]";
        out << ": " << issue.message << "\n";
    }

    if (issue.loc.line > 0) {
        if (color_) {
            out << LOG_DIM << "  --> " << LOG_RESET;
        } else {
            out << "  --> ";
        }
        if (!file.empty()) out << file << ":";
        out << issue.loc.line;
        if (issue.loc.column > 0) out << ":" << issue.loc.column;
        out << "\n";
    }

    // Source snippet + caret
    if (src_ && issue.loc.line > 0) {
        if (const std::string* text = src_->line_text(issue.loc.line)) {
            const std::string line_num = std::to_string(issue.loc.line);
            if (color_) {
                out << LOG_DIM << "  " << std::string(line_num.size(), ' ')
                    << " | " << LOG_RESET << *text << "\n";
                out << LOG_DIM << "  " << line_num << " | " << LOG_RESET;
                out << LOG_GREEN << "^" << LOG_RESET << "\n";
            } else {
                out << "  " << std::string(line_num.size(), ' ') << " | " << *text << "\n";
                out << "  " << line_num << " | " << "^\n";
            }
        }
    }

    for (const auto& n : issue.notes) {
        if (color_) out << LOG_CYAN << "  = note: " << LOG_RESET << n << "\n";
        else        out << "  = note: " << n << "\n";
    }
    if (!issue.suggestion.empty()) {
        if (color_) out << LOG_MAGENTA << "  = help: " << LOG_RESET << issue.suggestion << "\n";
        else        out << "  = help: " << issue.suggestion << "\n";
    }
}

// ===========================================================================
// Type
// ===========================================================================
bool Type::is_numeric() const {
    return kind == Kind::Primitive && is_numeric_name(name);
}

std::string Type::to_string() const {
    switch (kind) {
        case Kind::Unknown: return name.empty() ? "?" : name;
        case Kind::Any:     return "any";
        case Kind::None:    return "none";
        case Kind::Primitive: return name;
        case Kind::Struct:  return name;
        case Kind::Composite: {
            std::string s = name;
            if (!args.empty()) {
                s += "[";
                for (std::size_t i = 0; i < args.size(); ++i) {
                    if (i) s += ", ";
                    s += args[i] ? args[i]->to_string() : "?";
                }
                s += "]";
            }
            return s;
        }
        case Kind::Func: {
            std::string s = "(";
            for (std::size_t i = 0; i < args.size(); ++i) {
                if (i) s += ", ";
                s += args[i] ? args[i]->to_string() : "?";
            }
            s += ") -> ";
            s += ret ? ret->to_string() : "?";
            return s;
        }
    }
    return "?";
}

// ===========================================================================
// TypeRegistry
// ===========================================================================
void TypeRegistry::register_struct(const lmx::StructDeclNode* s) {
    if (s) structs_[s->name] = s;
}

void TypeRegistry::register_func(const lmx::FuncDeclNode* f) {
    if (f) funcs_[f->name] = f;
}

void TypeRegistry::register_lambda(const std::string& alias, const lmx::DoFuncDeclNode* /*f*/) {
    // Lambdas are anonymous; we only record an alias slot so that callers
    // can resolve a variable bound to a lambda as a function type. The actual
    // type is built on demand by infer().
    (void)alias;
}

bool TypeRegistry::is_known_type_name(const std::string& name) const {
    const std::string c = canonicalize(name);
    if (primitive_builtins().count(c)) return true;
    if (composite_builtins().count(c)) return true;
    if (structs_.count(c)) return true;
    return false;
}

const lmx::StructDeclNode* TypeRegistry::find_struct(const std::string& name) const {
    auto it = structs_.find(canonicalize(name));
    return it != structs_.end() ? it->second : nullptr;
}

const lmx::FuncDeclNode* TypeRegistry::find_func(const std::string& name) const {
    auto it = funcs_.find(name);
    return it != funcs_.end() ? it->second : nullptr;
}

std::string TypeRegistry::canonicalize(const std::string& name) const {
    const auto& m = alias_map();
    auto it = m.find(name);
    if (it != m.end()) return it->second;
    return name;
}

TypePtr TypeRegistry::make_type(const std::string& canonical_name) const {
    auto t = std::make_shared<Type>();
    t->name = canonical_name;
    if (canonical_name == "any") {
        t->kind = Type::Kind::Any;
    } else if (canonical_name == "nonetype" || canonical_name == "none") {
        t->kind = Type::Kind::None;
    } else if (composite_builtins().count(canonical_name)) {
        t->kind = Type::Kind::Composite;
        if (canonical_name == "Maybe") t->optional = true;
    } else if (auto* s = find_struct(canonical_name)) {
        t->kind = Type::Kind::Struct;
        t->struct_decl = s;
    } else if (primitive_builtins().count(canonical_name)) {
        t->kind = Type::Kind::Primitive;
    } else {
        // Unknown identifier referenced as a type — keep it as Unknown so the
        // caller can decide whether to emit a "did you mean ..." hint.
        t->kind = Type::Kind::Unknown;
    }
    return t;
}

TypePtr TypeRegistry::from_type_node(const lmx::TypeNode* node) const {
    if (!node) return std::make_shared<Type>(); // Unknown
    if (node->kind == lmx::ASTNodeType::CompositeType) {
        const auto* c = dynamic_cast<const lmx::CompositeTypeNode*>(node);
        std::vector<TypePtr> sub;
        sub.reserve(c->subtypes.size());
        for (const auto* s : c->subtypes) sub.push_back(from_type_node(s));
        auto t = make_type(canonicalize(c->name));
        t->kind = Type::Kind::Composite;
        t->args = std::move(sub);
        if (t->name == "Maybe") t->optional = true;
        return t;
    }
    return make_type(canonicalize(node->name));
}

TypePtr TypeRegistry::from_func_decl(const lmx::FuncDeclNode* f) const {
    auto t = std::make_shared<Type>();
    t->kind = Type::Kind::Func;
    t->name = f->name;
    t->args.reserve(f->params.size());
    for (const auto& p : f->params) {
        if (p.has_type && p.type_expr) t->args.push_back(from_type_node(p.type_expr));
        else                           t->args.push_back(std::make_shared<Type>()); // Unknown
    }
    if (f->ret_type) t->ret = from_type_node(f->ret_type);
    else             t->ret = std::make_shared<Type>();
    return t;
}

bool TypeRegistry::assignable(const TypePtr& expected, const TypePtr& actual) const {
    if (!expected || !actual) return true;
    if (expected->is_unknown() || actual->is_unknown()) return true;
    if (expected->is_any() || actual->is_any()) return true;
    if (expected->kind == Type::Kind::None && actual->kind == Type::Kind::None) return true;
    if (expected->optional && actual->kind == Type::Kind::None) return true;

    // Maybe[T]  ~  T  (an optional slot accepts the bare payload)
    if (expected->kind == Type::Kind::Composite && expected->name == "Maybe" &&
        expected->args.size() == 1) {
        if (assignable(expected->args[0], actual)) return true;
    }

    // Struct subtyping via `struct Sub : Base`
    if (expected->kind == Type::Kind::Struct && actual->kind == Type::Kind::Struct) {
        if (expected->name == actual->name) return true;
        if (actual->struct_decl && !actual->struct_decl->base_name.empty()) {
            const std::string base = canonicalize(actual->struct_decl->base_name);
            if (base == expected->name) return true;
        }
        return false;
    }
    // Struct that declares a primitive base (e.g. `struct SubLabel : text`)
    if (expected->kind == Type::Kind::Primitive && actual->kind == Type::Kind::Struct) {
        if (actual->struct_decl && !actual->struct_decl->base_name.empty()) {
            const std::string base = canonicalize(actual->struct_decl->base_name);
            if (base == expected->name) return true;
        }
        return false;
    }

    if (expected->kind == Type::Kind::Composite && actual->kind == Type::Kind::Composite) {
        if (expected->name != actual->name) {
            // bare vector ~ vec[T]
            if (expected->name == "vector" && actual->name == "vector") {
                if (expected->args.empty() || actual->args.empty()) return true;
                return assignable(expected->args[0], actual->args[0]);
            }
            return false;
        }
        if (expected->args.empty() || actual->args.empty()) return true;
        if (expected->args.size() != actual->args.size()) return false;
        for (std::size_t i = 0; i < expected->args.size(); ++i) {
            if (!assignable(expected->args[i], actual->args[i])) return false;
        }
        return true;
    }
    if (expected->kind == Type::Kind::Composite && actual->kind == Type::Kind::Primitive &&
        expected->name == "vector" && actual->name == "vector") {
        return true;
    }

    if (expected->kind == Type::Kind::Func && actual->kind == Type::Kind::Func) {
        if (expected->args.size() != actual->args.size()) return false;
        for (std::size_t i = 0; i < expected->args.size(); ++i)
            if (!assignable(expected->args[i], actual->args[i])) return false;
        if (expected->ret && actual->ret && !assignable(expected->ret, actual->ret)) return false;
        return true;
    }

    if (expected->kind == Type::Kind::Primitive && actual->kind == Type::Kind::Primitive) {
        if (expected->name == actual->name) return true;
        if (expected->is_numeric() && actual->is_numeric()) return true;
        return false;
    }

    return false;
}

std::string TypeRegistry::suggest_type_name(const std::string& misspelled) const {
    if (misspelled.empty()) return {};
    std::vector<std::string> pool;
    for (const auto& s : primitive_builtins()) pool.push_back(s);
    for (const auto& s : composite_builtins()) pool.push_back(s);
    for (const auto& kv : structs_) pool.push_back(kv.first);
    // also accept common aliases as suggestions
    for (const auto& kv : alias_map()) pool.push_back(kv.first);

    std::string best;
    int best_dist = std::numeric_limits<int>::max();
    for (const auto& cand : pool) {
        const int d = levenshtein(misspelled, cand);
        if (d < best_dist) { best_dist = d; best = cand; }
    }
    const int threshold = static_cast<int>(misspelled.size()) <= 4 ? 1 : 2;
    if (best_dist <= threshold) return best;
    return {};
}

const lmx::StructField* TypeRegistry::find_field(const TypePtr& struct_type,
                                                 const std::string& field_name) const {
    if (!struct_type || !struct_type->is_struct() || !struct_type->struct_decl) return nullptr;
    for (const auto& f : struct_type->struct_decl->fields) {
        if (f.name == field_name) return &f;
    }
    return nullptr;
}

std::string TypeRegistry::suggest_field_name(const TypePtr& struct_type,
                                             const std::string& misspelled) const {
    if (!struct_type || !struct_type->is_struct() || !struct_type->struct_decl) return {};
    std::string best;
    int best_dist = std::numeric_limits<int>::max();
    for (const auto& f : struct_type->struct_decl->fields) {
        const int d = levenshtein(misspelled, f.name);
        if (d < best_dist) { best_dist = d; best = f.name; }
    }
    const int threshold = static_cast<int>(misspelled.size()) <= 4 ? 1 : 2;
    return best_dist <= threshold ? best : std::string{};
}

// ===========================================================================
// Scope
// ===========================================================================
TypePtr Scope::lookup(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) return it->second.type;
    if (parent_) return parent_->lookup(name);
    return nullptr;
}

bool Scope::is_annotated(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) return it->second.annotated;
    if (parent_) return parent_->is_annotated(name);
    return false;
}

} // namespace olmcheck

// ===========================================================================
// Checker
// ===========================================================================
namespace olmcheck {

SourceLoc Checker::loc(int line) const {
    SourceLoc l; l.line = line; l.file = src_.file().empty() ? file_path_ : src_.file();
    return l;
}

void Checker::collect_symbols(const lmx::ASTNode* node) {
    std::unordered_map<std::string, const lmx::StructDeclNode*> structs;
    std::unordered_map<std::string, const lmx::FuncDeclNode*> funcs;
    ::collect_symbols(node, structs, funcs);
    for (const auto& kv : structs) registry_.register_struct(kv.second);
    for (const auto& kv : funcs)   registry_.register_func(kv.second);
}

// ---------------------------------------------------------------------------
// Inference
// ---------------------------------------------------------------------------
TypePtr Checker::infer(const lmx::ExprNode* expr, const Scope& scope) {
    if (!expr) return std::make_shared<Type>();

    if (dynamic_cast<const lmx::NumberNode*>(expr))
        return registry_.make_type("num");
    if (dynamic_cast<const lmx::StringNode*>(expr))
        return registry_.make_type("text");
    if (dynamic_cast<const lmx::BoolNode*>(expr))
        return registry_.make_type("bool");

    if (const auto* vec = dynamic_cast<const lmx::VectorNode*>(expr)) {
        auto t = registry_.make_type("vector");
        t->kind = Type::Kind::Composite;
        TypePtr elt;
        for (const auto* e : vec->elements) {
            if (const auto* ex = dynamic_cast<const lmx::ExprNode*>(e)) {
                TypePtr et = infer(ex, scope);
                if (!elt) elt = et;
                else if (elt->is_unknown() && !et->is_unknown()) elt = et;
            }
        }
        t->args.push_back(elt ? elt : std::make_shared<Type>());
        return t;
    }

    if (dynamic_cast<const lmx::DictionaryNode*>(expr))
        return registry_.make_type("table");

    if (const auto* ref = dynamic_cast<const lmx::VarRefNode*>(expr)) {
        if (TypePtr t = scope.lookup(ref->name)) return t;
        if (auto* s = registry_.find_struct(ref->name)) {
            auto st = registry_.make_type(s->name);
            return st;
        }
        return std::make_shared<Type>();
    }

    if (const auto* bin = dynamic_cast<const lmx::BinaryNode*>(expr)) {
        TypePtr l = infer(bin->left, scope);
        TypePtr r = infer(bin->right, scope);
        if (bin->op == "+" || bin->op == "-" || bin->op == "*" ||
            bin->op == "/" || bin->op == "%") {
            if (l->is_numeric() && r->is_numeric()) return registry_.make_type("num");
            if (l->is_text() && r->is_text() && bin->op == "+") return registry_.make_type("text");
        }
        if (bin->op == "==" || bin->op == "!=" || bin->op == "<" || bin->op == ">" ||
            bin->op == "<=" || bin->op == ">=")
            return registry_.make_type("bool");
        if (bin->op == "and" || bin->op == "or") {
            if (l->is_bool() && r->is_bool()) return registry_.make_type("bool");
            if (l->is_bool()) return r;
            if (r->is_bool()) return l;
        }
        if (bin->op == "not") return registry_.make_type("bool");
        if (bin->op == ".") return l;
        return std::make_shared<Type>();
    }

    if (const auto* u = dynamic_cast<const lmx::UnaryNode*>(expr)) {
        TypePtr o = infer(u->operand, scope);
        if (u->op == "-") {
            if (o->is_numeric()) return registry_.make_type("num");
        }
        if (u->op == "not") return registry_.make_type("bool");
        return o;
    }

    if (const auto* call = dynamic_cast<const lmx::FuncCallExprNode*>(expr)) {
        if (const auto* ref = dynamic_cast<const lmx::VarRefNode*>(call->func_expr)) {
            if (auto* s = registry_.find_struct(ref->name)) {
                return registry_.make_type(s->name);
            }
            if (auto* f = registry_.find_func(ref->name)) {
                TypePtr ft = registry_.from_func_decl(f);
                return ft->ret ? ft->ret : std::make_shared<Type>();
            }
        }
        return std::make_shared<Type>();
    }

    if (const auto* mem = dynamic_cast<const lmx::MemberAccessNode*>(expr)) {
        TypePtr obj = infer(mem->object, scope);
        if (obj && obj->is_struct()) {
            if (const lmx::StructField* f = registry_.find_field(obj, mem->member)) {
                if (f->has_type_annotation && f->type_expr)
                    return registry_.from_type_node(f->type_expr);
            }
        }
        // common vector / table methods
        if (obj && (obj->name == "vector" || obj->name == "table")) {
            if (mem->member == "len") return registry_.make_type("num");
        }
        return std::make_shared<Type>();
    }

    if (const auto* idx = dynamic_cast<const lmx::IndexAccessNode*>(expr)) {
        TypePtr obj = infer(idx->object, scope);
        if (obj && obj->kind == Type::Kind::Composite && obj->name == "vector" &&
            !obj->args.empty()) {
            return obj->args[0];
        }
        return std::make_shared<Type>();
    }

    if (const auto* lam = dynamic_cast<const lmx::DoFuncDeclNode*>(expr)) {
        auto t = std::make_shared<Type>();
        t->kind = Type::Kind::Func;
        t->name = "<lambda>";
        for (const auto& p : lam->params) {
            if (p.has_type && p.type_expr) t->args.push_back(registry_.from_type_node(p.type_expr));
            else                           t->args.push_back(std::make_shared<Type>());
        }
        if (lam->ret_type) t->ret = registry_.from_type_node(lam->ret_type);
        return t;
    }

    if (const auto* comp = dynamic_cast<const lmx::ComprehensionNode*>(expr)) {
        auto t = registry_.make_type("vector");
        t->kind = Type::Kind::Composite;
        // Best-effort element type: infer the mapped expression under a scope
        // that binds the iteration variables to `any`.
        Scope inner(&scope);
        for (const auto* item : comp->items) {
            if (item) inner.define(item->var_name, std::make_shared<Type>());
        }
        t->args.push_back(infer(comp->expr, inner));
        return t;
    }

    return std::make_shared<Type>();
}

// ---------------------------------------------------------------------------
// Diagnostics helper
// ---------------------------------------------------------------------------
void Checker::emit_type_mismatch(const SourceLoc& l, const std::string& context,
                                 const TypePtr& expected, const TypePtr& actual,
                                 std::string suggestion) {
    std::ostringstream ss;
    ss << context << ": expected `" << expected->to_string()
       << "` but got `" << actual->to_string() << "`";
    reporter_.error(l, ss.str(), "type-mismatch", std::move(suggestion));
    if (view_log_) {
        std::clog << LOG_RED << "[MISMATCH] " << LOG_RESET << context
                  << " expected=" << expected->to_string()
                  << " actual=" << actual->to_string() << "\n";
    }
}

// ---------------------------------------------------------------------------
// Statement / declaration checking
// ---------------------------------------------------------------------------
void Checker::check_program(const lmx::ProgramASTNode* prog) {
    if (!prog) return;
    src_.set_lines(prog->source_lines);
    src_.set_file(prog->source_filename.empty() ? file_path_ : prog->source_filename);
    reporter_.set_source(&src_);

    Scope top;
    for (const auto* stmt : prog->stmts) check_node(stmt, top, nullptr);
}

void Checker::check_block(const lmx::BlockStmtNode* block, Scope& parent,
                          const TypePtr& current_ret) {
    if (!block) return;
    Scope inner(&parent);
    for (const auto* s : block->stmts) check_node(s, inner, current_ret);
}

void Checker::check_node(const lmx::ASTNode* node, Scope& scope,
                         const TypePtr& current_ret) {
    if (!node) return;

    if (const auto* prog = dynamic_cast<const lmx::ProgramASTNode*>(node)) {
        for (const auto* s : prog->stmts) check_node(s, scope, current_ret);
    } else if (const auto* f = dynamic_cast<const lmx::FuncDeclNode*>(node)) {
        check_func(f);
    } else if (const auto* s = dynamic_cast<const lmx::StructDeclNode*>(node)) {
        check_struct(s);
    } else if (const auto* v = dynamic_cast<const lmx::VarDeclNode*>(node)) {
        check_var_decl(v, scope, current_ret);
    } else if (const auto* b = dynamic_cast<const lmx::BlockStmtNode*>(node)) {
        check_block(b, scope, current_ret);
    } else if (const auto* m = dynamic_cast<const lmx::ModuleNode*>(node)) {
        check_module(m, scope, current_ret);
    } else if (const auto* ifs = dynamic_cast<const lmx::IfStmtNode*>(node)) {
        check_if(ifs, scope, current_ret);
    } else if (const auto* loop = dynamic_cast<const lmx::ForLoopNode*>(node)) {
        check_for(loop, scope, current_ret);
    } else if (const auto* wh = dynamic_cast<const lmx::WhileStmtNode*>(node)) {
        check_while(wh, scope, current_ret);
    } else if (const auto* lp = dynamic_cast<const lmx::LoopNode*>(node)) {
        check_loop(lp, scope, current_ret);
    } else if (const auto* ret = dynamic_cast<const lmx::ReturnStmtNode*>(node)) {
        check_return(ret, scope, current_ret);
    } else if (const auto* a = dynamic_cast<const lmx::AssignNode*>(node)) {
        check_assign(a, scope, current_ret);
    } else if (const auto* dec = dynamic_cast<const lmx::DecoratedFuncNode*>(node)) {
        check_node(dec->target, scope, current_ret);
    } else if (const auto* expr = dynamic_cast<const lmx::ExprNode*>(node)) {
        check_expr_node(expr, scope);
    }
}

void Checker::check_module(const lmx::ModuleNode* mod, Scope& scope,
                           const TypePtr& current_ret) {
    if (!mod) return;
    Scope inner(&scope);
    for (const auto* v : mod->vars)   check_node(v, inner, current_ret);
    for (const auto* f : mod->ord_funcs) check_node(f, inner, current_ret);
    for (const auto* c : mod->children) check_node(c, inner, current_ret);
}

void Checker::check_if(const lmx::IfStmtNode* ifs, Scope& scope,
                       const TypePtr& current_ret) {
    if (!ifs) return;
    if (ifs->condition) {
        TypePtr c = infer(ifs->condition, scope);
        if (!c->is_unknown() && !c->is_any() && !c->is_bool()) {
            emit_type_mismatch(loc(ifs->condition->source_line),
                "if condition", registry_.make_type("bool"), c,
                "conditions must be `bool`");
        }
        check_expr_node(ifs->condition, scope);
    }
    check_block(ifs->then_block, scope, current_ret);
    for (const auto& e : ifs->elif_blocks) {
        if (e.condition) {
            TypePtr c = infer(e.condition, scope);
            if (!c->is_unknown() && !c->is_any() && !c->is_bool()) {
                emit_type_mismatch(loc(e.condition->source_line),
                    "elif condition", registry_.make_type("bool"), c);
            }
            check_expr_node(e.condition, scope);
        }
        check_block(e.block, scope, current_ret);
    }
    check_block(ifs->else_block, scope, current_ret);
}

void Checker::check_for(const lmx::ForLoopNode* loop, Scope& scope,
                        const TypePtr& current_ret) {
    if (!loop) return;
    Scope inner(&scope);
    for (const auto* item : loop->items) {
        if (!item) continue;
        TypePtr it_t;
        if (item->iterable) {
            TypePtr coll = infer(item->iterable, scope);
            if (coll && coll->kind == Type::Kind::Composite && coll->name == "vector" &&
                !coll->args.empty()) {
                it_t = coll->args[0];
            }
        }
        inner.define(item->var_name, it_t ? it_t : std::make_shared<Type>());
        if (item->iterable) check_expr_node(item->iterable, scope);
    }
    check_block(loop->body, inner, current_ret);
}

void Checker::check_while(const lmx::WhileStmtNode* wh, Scope& scope,
                          const TypePtr& current_ret) {
    if (!wh) return;
    if (wh->condition) {
        TypePtr c = infer(wh->condition, scope);
        if (!c->is_unknown() && !c->is_any() && !c->is_bool()) {
            emit_type_mismatch(loc(wh->condition->source_line),
                "while condition", registry_.make_type("bool"), c);
        }
        check_expr_node(wh->condition, scope);
    }
    check_block(wh->body, scope, current_ret);
}

void Checker::check_loop(const lmx::LoopNode* lp, Scope& scope,
                         const TypePtr& current_ret) {
    if (!lp) return;
    if (lp->condition) {
        TypePtr c = infer(lp->condition, scope);
        if (!c->is_unknown() && !c->is_any() && !c->is_bool()) {
            emit_type_mismatch(loc(lp->condition->source_line),
                "loop condition", registry_.make_type("bool"), c);
        }
        check_expr_node(lp->condition, scope);
    }
    check_block(lp->body, scope, current_ret);
}

void Checker::check_return(const lmx::ReturnStmtNode* ret, Scope& scope,
                           const TypePtr& current_ret) {
    if (!ret) return;
    TypePtr actual = ret->expr ? infer(ret->expr, scope) : registry_.make_type("nonetype");
    if (ret->expr) check_expr_node(ret->expr, scope);
    if (!current_ret || current_ret->is_unknown() || current_ret->is_any()) {
        // No declared return type — infer it for a later hint at the function level.
        return;
    }
    if (current_ret->optional && actual->kind == Type::Kind::None) return;
    if (registry_.assignable(current_ret, actual)) {
        if (view_log_) view_log_match("return", current_ret, actual, ret->source_line);
        return;
    }
    emit_type_mismatch(loc(ret->source_line), "return type",
                       current_ret, actual,
                       "declared return type of this function");
}

void Checker::check_var_decl(const lmx::VarDeclNode* var, Scope& scope,
                             const TypePtr& /*current_ret*/) {
    if (!var) return;

    // Optional explicit type annotation: `let a: Type = expr`.
    // When present, the initializer is checked against it and the binding is
    // recorded as `annotated` so subsequent assignments are also checked.
    // When absent, the variable stays dynamic: its inferred type is used only
    // for downstream inference, and reassignments are not type-checked.
    if (var->has_type_annotation && var->type_annotation) {
        const std::string raw = var->type_annotation->name;
        if (!registry_.is_known_type_name(raw)) {
            std::string sug = registry_.suggest_type_name(raw);
            std::string help = sug.empty() ? std::string{}
                : "did you mean `" + sug + "`?";
            reporter_.error(loc(var->type_annotation->source_line
                    ? var->type_annotation->source_line : var->source_line),
                "unknown type `" + raw + "` used in declaration of `" + var->name + "`",
                "unknown-type", help);
        }
        TypePtr declared = registry_.from_type_node(var->type_annotation);

        if (var->init) {
            TypePtr init_t = infer(var->init, scope);
            check_expr_node(var->init, scope);
            // `let a: num = "text"` is a real error; `let a: num = 1` is fine.
            if (!registry_.assignable(declared, init_t) &&
                !init_t->is_unknown() && !init_t->is_any()) {
                emit_type_mismatch(loc(var->init->source_line),
                    "initializer of `" + var->name + "`",
                    declared, init_t);
            }
        }
        scope.define(var->name, declared, true);
        return;
    }

    // Unannotated: infer for inference only, keep dynamic for assignment checks.
    TypePtr t = std::make_shared<Type>();
    if (var->init) {
        t = infer(var->init, scope);
        check_expr_node(var->init, scope);
    }
    scope.define(var->name, t, false);
}

void Checker::check_assign(const lmx::AssignNode* assign, Scope& scope,
                           const TypePtr& current_ret) {
    if (!assign) return;
    TypePtr value_t = infer(assign->value, scope);
    check_expr_node(assign->value, scope);
    if (const auto* ref = dynamic_cast<const lmx::VarRefNode*>(assign->var)) {
        // Only enforce the type on reassignment when the variable was declared
        // with an explicit annotation. Unannotated variables stay dynamic, so
        // `let a = 1; a = "a"` is *not* a problem.
        if (scope.is_annotated(ref->name)) {
            if (TypePtr lhs = scope.lookup(ref->name)) {
                if (!registry_.assignable(lhs, value_t) &&
                    !lhs->is_unknown() && !value_t->is_unknown() &&
                    !value_t->is_any()) {
                    emit_type_mismatch(loc(assign->source_line),
                        "assignment to `" + ref->name + "`", lhs, value_t);
                }
            }
        }
    } else if (const auto* mem = dynamic_cast<const lmx::MemberAccessNode*>(assign->var)) {
        TypePtr obj = infer(mem->object, scope);
        if (obj && obj->is_struct()) {
            if (const lmx::StructField* f = registry_.find_field(obj, mem->member)) {
                if (f->has_type_annotation && f->type_expr) {
                    TypePtr ft = registry_.from_type_node(f->type_expr);
                    if (!registry_.assignable(ft, value_t) && !value_t->is_unknown()) {
                        emit_type_mismatch(loc(assign->source_line),
                            "assignment to field `" + obj->name + "." + mem->member + "`",
                            ft, value_t);
                    }
                }
            } else {
                reporter_.error(loc(assign->source_line),
                    "struct `" + obj->name + "` has no field `" + mem->member + "`",
                    "no-such-field",
                    registry_.suggest_field_name(obj, mem->member));
            }
        }
    } else if (const auto* idx = dynamic_cast<const lmx::IndexAccessNode*>(assign->var)) {
        check_expr_node(idx->object, scope);
        check_expr_node(idx->index, scope);
    }
    (void)current_ret;
}

// ---------------------------------------------------------------------------
// Function / struct checking (with type-hint suggestions)
// ---------------------------------------------------------------------------
namespace {
/// Lightweight usage-based inference for an untyped parameter: look at how
/// the parameter is used inside the body and guess a primitive type.
TypePtr guess_param_type_from_usage(const std::string& name,
                                    const lmx::BlockStmtNode* body) {
    if (!body) return nullptr;
    TypePtr guess;
    std::function<void(const lmx::ASTNode*)> walk = [&](const lmx::ASTNode* n) {
        if (!n || guess) return;
        if (const auto* bin = dynamic_cast<const lmx::BinaryNode*>(n)) {
            auto is_ref = [&](const lmx::ExprNode* e) {
                const auto* r = dynamic_cast<const lmx::VarRefNode*>(e);
                return r && r->name == name;
            };
            if (bin->op == "+" || bin->op == "-" || bin->op == "*" ||
                bin->op == "/" || bin->op == "%") {
                if (is_ref(bin->left) || is_ref(bin->right)) {
                    guess = std::make_shared<Type>();
                    guess->kind = Type::Kind::Primitive; guess->name = "num";
                    return;
                }
            }
            // text concatenation: x + "literal"
            if (bin->op == "+" &&
                ((is_ref(bin->left) && dynamic_cast<const lmx::StringNode*>(bin->right)) ||
                 (is_ref(bin->right) && dynamic_cast<const lmx::StringNode*>(bin->left)))) {
                guess = std::make_shared<Type>();
                guess->kind = Type::Kind::Primitive; guess->name = "text";
                return;
            }
            walk(bin->left); walk(bin->right);
        } else if (const auto* u = dynamic_cast<const lmx::UnaryNode*>(n)) {
            const auto* r = dynamic_cast<const lmx::VarRefNode*>(u->operand);
            if (u->op == "not" && r && r->name == name) {
                guess = std::make_shared<Type>();
                guess->kind = Type::Kind::Primitive; guess->name = "bool";
                return;
            }
            walk(u->operand);
        } else if (const auto* b = dynamic_cast<const lmx::BlockStmtNode*>(n)) {
            for (const auto* s : b->stmts) walk(s);
        } else if (const auto* ifs = dynamic_cast<const lmx::IfStmtNode*>(n)) {
            if (ifs->then_block) for (const auto* s : ifs->then_block->stmts) walk(s);
            if (ifs->else_block) for (const auto* s : ifs->else_block->stmts) walk(s);
        }
    };
    for (const auto* s : body->stmts) walk(s);
    return guess;
}
} // namespace

TypePtr Checker::infer_first_return_type(const lmx::FuncDeclNode* f, const Scope& base) {
    if (!f || !f->body) return nullptr;
    for (const auto* s : f->body->stmts) {
        if (const auto* r = dynamic_cast<const lmx::ReturnStmtNode*>(s)) {
            if (r->expr) return infer(r->expr, base);
            return std::make_shared<Type>(); // Unknown
        }
    }
    return nullptr;
}

void Checker::check_func(const lmx::FuncDeclNode* func) {
    if (!func) return;
    Scope fn_scope;
    for (const auto& p : func->params) {
        TypePtr pt;
        if (p.has_type && p.type_expr) {
            // Validate the annotation's type name is known.
            const std::string c = registry_.canonicalize(p.type_expr->name);
            if (!registry_.is_known_type_name(p.type_expr->name) &&
                !registry_.find_struct(p.type_expr->name)) {
                std::string sug = registry_.suggest_type_name(p.type_expr->name);
                std::string help = sug.empty() ? std::string{}
                    : "did you mean `" + sug + "`?";
                reporter_.error(loc(p.type_expr->source_line ? p.type_expr->source_line : func->source_line),
                    "unknown type `" + p.type_expr->name + "` used in parameter `" + p.name + "`",
                    "unknown-type", help);
            }
            pt = registry_.from_type_node(p.type_expr);
        }

        // Check default value against the annotation, if any.
        if (p.default_value) {
            TypePtr def_t = infer(p.default_value, fn_scope);
            check_expr_node(p.default_value, fn_scope);
            if (pt && !pt->is_unknown()) {
                if (registry_.assignable(pt, def_t)) {
                    if (view_log_) view_log_match(
                        "param `" + p.name + "` default", pt, def_t, p.default_value->source_line);
                } else {
                    emit_type_mismatch(loc(p.default_value->source_line),
                        "default value of parameter `" + p.name + "`",
                        pt, def_t);
                }
            } else if (!def_t->is_unknown() && !def_t->is_any() &&
                       def_t->kind != Type::Kind::Unknown) {
                // Untyped parameter with a default: suggest the inferred type.
                reporter_.hint(loc(p.default_value->source_line),
                    "parameter `" + p.name + "` has no type annotation; "
                    "inferred `" + def_t->to_string() + "` from its default value");
            }
        } else if (!pt || pt->is_unknown()) {
            // No annotation and no default: try to guess from body usage.
            if (TypePtr g = guess_param_type_from_usage(p.name, func->body)) {
                reporter_.hint(loc(func->source_line),
                    "parameter `" + p.name + "` has no type annotation; "
                    "inferred `" + g->to_string() + "` from its usage in the body");
                pt = g;
            }
        }

        if (!pt) pt = std::make_shared<Type>();
        fn_scope.define(p.name, pt);
    }

    TypePtr ret_t;
    if (func->ret_type) {
        if (!registry_.is_known_type_name(func->ret_type->name) &&
            !registry_.find_struct(func->ret_type->name)) {
            std::string sug = registry_.suggest_type_name(func->ret_type->name);
            std::string help = sug.empty() ? std::string{}
                : "did you mean `" + sug + "`?";
            reporter_.error(loc(func->ret_type->source_line ? func->ret_type->source_line : func->source_line),
                "unknown return type `" + func->ret_type->name + "`",
                "unknown-type", help);
        }
        ret_t = registry_.from_type_node(func->ret_type);
    } else {
        ret_t = nullptr;
    }

    if (func->body) check_block(func->body, fn_scope, ret_t);

    // Suggest a return annotation when one is missing but consistent.
    if (!func->ret_type && func->body) {
        TypePtr first = infer_first_return_type(func, fn_scope);
        if (first && !first->is_unknown() && !first->is_any() &&
            first->kind != Type::Kind::Unknown) {
            reporter_.hint(loc(func->source_line),
                "function `" + func->name +
                "` has no declared return type; inferred `" + first->to_string() + "`");
        }
    }
}

void Checker::check_struct(const lmx::StructDeclNode* s) {
    if (!s) return;
    // Type parameters declared on this struct (e.g. `[t: num]`) are valid
    // type names within its own field annotations.
    std::unordered_set<std::string> type_params;
    for (const auto& tp : s->type_params) type_params.insert(tp.name);

    std::set<std::string> seen;
    for (const auto& f : s->fields) {
        if (seen.count(f.name)) {
            reporter_.error(loc(f.default_init ? f.default_init->source_line : s->source_line),
                "struct `" + s->name + "`: duplicate field `" + f.name + "`",
                "duplicate-field");
        }
        seen.insert(f.name);

        TypePtr ft;
        if (f.has_type_annotation && f.type_expr) {
            if (!type_params.count(f.type_expr->name) &&
                !registry_.is_known_type_name(f.type_expr->name)) {
                std::string sug = registry_.suggest_type_name(f.type_expr->name);
                std::string help = sug.empty() ? std::string{}
                    : "did you mean `" + sug + "`?";
                reporter_.error(loc(f.type_expr->source_line ? f.type_expr->source_line : s->source_line),
                    "struct `" + s->name + "`: field `" + f.name +
                    "` has unknown type `" + f.type_expr->name + "`",
                    "unknown-type", help);
            }
            ft = registry_.from_type_node(f.type_expr);
        }

        if (f.default_init) {
            Scope empty;
            TypePtr def_t = infer(f.default_init, empty);
            check_expr_node(f.default_init, empty);
            if (ft && !ft->is_unknown()) {
                if (registry_.assignable(ft, def_t)) {
                    if (view_log_) view_log_match(
                        "field `" + s->name + "." + f.name + "` default",
                        ft, def_t, f.default_init->source_line);
                } else {
                    emit_type_mismatch(loc(f.default_init->source_line),
                        "default value of field `" + s->name + "." + f.name + "`",
                        ft, def_t);
                }
            } else if (!def_t->is_unknown() && !def_t->is_any() &&
                       def_t->kind != Type::Kind::Unknown) {
                reporter_.hint(loc(f.default_init->source_line),
                    "field `" + f.name + "` has no type annotation; "
                    "inferred `" + def_t->to_string() + "` from its default value");
            }
        }
    }

    // Methods are checked as ordinary functions.
    for (const auto* m : s->methods) check_func(m);
}

void Checker::check_struct_construct(const lmx::FuncCallExprNode* call,
                                     const lmx::StructDeclNode* s,
                                     const Scope& scope) {
    const auto& fields = s->fields;
    const std::size_t positional = std::count_if(call->args.begin(), call->args.end(),
        [](const lmx::CallArgument& a) { return a.name.empty() && !a.is_splat; });
    if (positional > fields.size()) {
        reporter_.error(loc(call->source_line),
            "struct `" + s->name + "` expects at most " +
            std::to_string(fields.size()) + " field(s) but got " +
            std::to_string(positional) + " argument(s)",
            "arity-mismatch");
    }

    std::size_t pos = 0;
    for (const auto& arg : call->args) {
        if (!arg.value) continue;
        const lmx::StructField* field = nullptr;
        if (arg.name.empty()) {
            if (pos < fields.size()) field = &fields[pos];
            ++pos;
        } else {
            for (const auto& fl : fields) if (fl.name == arg.name) { field = &fl; break; }
            if (!field) {
                reporter_.error(loc(arg.value->source_line),
                    "struct `" + s->name + "` has no field `" + arg.name + "`",
                    "no-such-field",
                    registry_.suggest_field_name(registry_.make_type(s->name), arg.name));
            }
        }
        if (field && field->has_type_annotation && field->type_expr) {
            TypePtr ft = registry_.from_type_node(field->type_expr);
            TypePtr at = infer(arg.value, scope);
            if (!registry_.assignable(ft, at) && !at->is_unknown() && !at->is_any()) {
                emit_type_mismatch(loc(arg.value->source_line),
                    "argument for field `" + s->name + "." + field->name + "`",
                    ft, at);
            }
        }
        check_expr_node(arg.value, scope);
    }
}

// ---------------------------------------------------------------------------
// Expression walking (diagnostics only; inference is separate)
// ---------------------------------------------------------------------------
void Checker::check_expr_node(const lmx::ExprNode* expr, const Scope& scope) {
    if (!expr) return;

    if (const auto* bin = dynamic_cast<const lmx::BinaryNode*>(expr)) {
        TypePtr l = infer(bin->left, scope);
        TypePtr r = infer(bin->right, scope);
        if (bin->op == "+" || bin->op == "-" || bin->op == "*" ||
            bin->op == "/" || bin->op == "%") {
            const bool lnum = l->is_numeric() || l->is_unknown() || l->is_any();
            const bool rnum = r->is_numeric() || r->is_unknown() || r->is_any();
            if (!lnum || !rnum) {
                if (!(l->is_text() && r->is_text() && bin->op == "+")) {
                    emit_type_mismatch(loc(bin->source_line),
                        "operator `" + bin->op + "`",
                        registry_.make_type("num"), l,
                        "arithmetic operators require numeric operands");
                    (void)r;
                }
            }
        } else if (bin->op == "and" || bin->op == "or") {
            if (!l->is_unknown() && !l->is_any() && !l->is_bool()) {
                emit_type_mismatch(loc(bin->source_line),
                    "left operand of `" + bin->op + "`",
                    registry_.make_type("bool"), l);
            }
            if (!r->is_unknown() && !r->is_any() && !r->is_bool()) {
                emit_type_mismatch(loc(bin->source_line),
                    "right operand of `" + bin->op + "`",
                    registry_.make_type("bool"), r);
            }
        }
        check_expr_node(bin->left, scope);
        check_expr_node(bin->right, scope);
        return;
    }

    if (const auto* u = dynamic_cast<const lmx::UnaryNode*>(expr)) {
        TypePtr o = infer(u->operand, scope);
        if (u->op == "-" && !o->is_numeric() && !o->is_unknown() && !o->is_any()) {
            emit_type_mismatch(loc(u->source_line),
                "unary `-`", registry_.make_type("num"), o);
        }
        if (u->op == "not" && !o->is_bool() && !o->is_unknown() && !o->is_any()) {
            emit_type_mismatch(loc(u->source_line),
                "operator `not`", registry_.make_type("bool"), o);
        }
        check_expr_node(u->operand, scope);
        return;
    }

    if (const auto* call = dynamic_cast<const lmx::FuncCallExprNode*>(expr)) {
        if (const auto* ref = dynamic_cast<const lmx::VarRefNode*>(call->func_expr)) {
            if (auto* s = registry_.find_struct(ref->name)) {
                check_struct_construct(call, s, scope);
                return;
            }
            if (auto* f = registry_.find_func(ref->name)) {
                // Arity & per-argument check against declared parameter types.
                const auto& params = f->params;
                std::size_t pos = 0;
                for (const auto& a : call->args) {
                    if (!a.value) continue;
                    TypePtr pt;
                    if (a.name.empty()) {
                        if (pos < params.size() && params[pos].has_type && params[pos].type_expr)
                            pt = registry_.from_type_node(params[pos].type_expr);
                        ++pos;
                    } else {
                        for (const auto& prm : params) if (prm.name == a.name) {
                            if (prm.has_type && prm.type_expr)
                                pt = registry_.from_type_node(prm.type_expr);
                            break;
                        }
                    }
                    if (pt && !pt->is_unknown()) {
                        TypePtr at = infer(a.value, scope);
                        if (!registry_.assignable(pt, at) && !at->is_unknown() && !at->is_any()) {
                            emit_type_mismatch(loc(a.value->source_line),
                                "argument to `" + f->name + "`",
                                pt, at);
                        }
                    }
                    check_expr_node(a.value, scope);
                }
                const bool variadic = !params.empty() && params.back().is_variadic;
                if (!variadic && pos > params.size()) {
                    reporter_.warning(loc(call->source_line),
                        "function `" + f->name + "` expects " +
                        std::to_string(params.size()) + " argument(s) but got " +
                        std::to_string(pos),
                        "arity-mismatch");
                }
                return;
            }
        }
        // Unknown callee — just recurse into arguments.
        check_expr_node(call->func_expr, scope);
        for (const auto& a : call->args) check_expr_node(a.value, scope);
        return;
    }

    if (const auto* mem = dynamic_cast<const lmx::MemberAccessNode*>(expr)) {
        TypePtr obj = infer(mem->object, scope);
        check_expr_node(mem->object, scope);
        if (obj && obj->is_struct()) {
            if (!registry_.find_field(obj, mem->member)) {
                reporter_.error(loc(mem->source_line),
                    "struct `" + obj->name + "` has no field `" + mem->member + "`",
                    "no-such-field",
                    registry_.suggest_field_name(obj, mem->member));
            }
        }
        return;
    }

    if (const auto* idx = dynamic_cast<const lmx::IndexAccessNode*>(expr)) {
        TypePtr obj = infer(idx->object, scope);
        check_expr_node(idx->object, scope);
        check_expr_node(idx->index, scope);
        TypePtr it = infer(idx->index, scope);
        if (obj && obj->name == "table" && !it->is_unknown() && !it->is_any() &&
            !it->is_text() && !it->is_numeric()) {
            emit_type_mismatch(loc(idx->source_line),
                "table index", registry_.make_type("text"), it);
        }
        if (obj && obj->name == "vector" && !it->is_unknown() && !it->is_any() &&
            !it->is_numeric()) {
            emit_type_mismatch(loc(idx->source_line),
                "vector index", registry_.make_type("num"), it);
        }
        return;
    }

    if (const auto* vec = dynamic_cast<const lmx::VectorNode*>(expr)) {
        TypePtr common;
        bool warned = false;
        for (const auto* e : vec->elements) {
            if (const auto* ex = dynamic_cast<const lmx::ExprNode*>(e)) {
                TypePtr et = infer(ex, scope);
                check_expr_node(ex, scope);
                if (!common) common = et;
                else if (!et->is_unknown() && !et->is_any() && !registry_.assignable(common, et) &&
                         !warned) {
                    reporter_.warning(loc(ex->source_line),
                        "vector elements have inconsistent types: `" +
                        common->to_string() + "` and `" + et->to_string() + "`",
                        "heterogeneous-vector",
                        "consider a `vec[T]` annotation or `Union[...]`");
                    warned = true;
                }
            }
        }
        return;
    }

    if (const auto* dict = dynamic_cast<const lmx::DictionaryNode*>(expr)) {
        for (const auto* entry : dict->entries) {
            check_expr_node(entry->key, scope);
            check_expr_node(entry->value, scope);
        }
        return;
    }

    if (const auto* conv = dynamic_cast<const lmx::TypeConvertExprNode*>(expr)) {
        check_expr_node(conv->value_expr, scope);
        return;
    }
    if (const auto* lam = dynamic_cast<const lmx::DoFuncDeclNode*>(expr)) {
        // Treat the lambda like a function for body checking.
        Scope lscope;
        for (const auto& p : lam->params) {
            TypePtr pt = (p.has_type && p.type_expr) ? registry_.from_type_node(p.type_expr)
                                                     : std::make_shared<Type>();
            lscope.define(p.name, pt);
        }
        TypePtr rt = lam->ret_type ? registry_.from_type_node(lam->ret_type) : nullptr;
        if (lam->body) check_block(lam->body, lscope, rt);
        return;
    }
    if (const auto* comp = dynamic_cast<const lmx::ComprehensionNode*>(expr)) {
        Scope inner(&scope);
        for (const auto* item : comp->items)
            if (item) inner.define(item->var_name, std::make_shared<Type>());
        check_expr_node(comp->expr, inner);
        if (comp->guard) check_expr_node(comp->guard, inner);
        return;
    }
    if (const auto* mc = dynamic_cast<const lmx::MacroCallExprNode*>(expr)) {
        for (const auto& a : mc->args) check_expr_node(a.value, scope);
        return;
    }
}

// ---------------------------------------------------------------------------
// Top-level check + run_check
// ---------------------------------------------------------------------------
int Checker::check(const lmx::ProgramASTNode* ast) {
    if (!ast) return 1;
    collect_symbols(ast);
    check_program(ast);
    return static_cast<int>(reporter_.error_count());
}

int run_check(const std::string& file_path, bool verbose_log) {
    g_view_log_enabled = verbose_log;

    Reporter reporter;
    reporter.set_color(true);
    reporter.set_view_log(verbose_log);

    if (verbose_log) view_log_info("Starting type checker (verbose)");

    std::string source;
    try {
        std::ifstream f(file_path);
        if (!f) {
            std::cerr << "[OLMCheck] Error: cannot open file: " << file_path << "\n";
            return 1;
        }
        std::stringstream ss; ss << f.rdbuf();
        source = ss.str();
    } catch (const std::exception& e) {
        std::cerr << "[OLMCheck] Error reading file: " << e.what() << "\n";
        return 1;
    }

    lang::lammp::ensure_lmmp_initialized();
    lmx::ProgramASTNode* ast = parse(source, file_path);
    if (!ast) {
        std::cerr << "[OLMCheck] Error: parsing failed\n";
        return 1;
    }
    if (verbose_log) view_log_info("AST parsed; beginning check");

    Checker checker(reporter, file_path);
    checker.set_view_log(verbose_log);
    const int errors = checker.check(ast);
    delete ast;

    if (errors > 0) {
        std::cerr << "[OLMCheck] " << errors << " error(s)";
        const std::size_t w = reporter.warning_count();
        if (w) std::cerr << ", " << w << " warning(s)";
        std::cerr << ".\n";
    } else {
        std::cout << "[OLMCheck] No issues found.\n";
    }
    return errors;
}

} // namespace olmcheck
