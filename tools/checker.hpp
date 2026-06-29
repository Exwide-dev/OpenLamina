#pragma once

#include "parser/ast.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace olmcheck {

// ---------------------------------------------------------------------------
// ANSI terminal styling
// ---------------------------------------------------------------------------
constexpr const char* LOG_RESET   = "\033[0m";
constexpr const char* LOG_DIM     = "\033[2m";
constexpr const char* LOG_BOLD    = "\033[1m";
constexpr const char* LOG_GREEN   = "\033[32m";
constexpr const char* LOG_RED     = "\033[31m";
constexpr const char* LOG_YELLOW  = "\033[33m";
constexpr const char* LOG_BLUE    = "\033[34m";
constexpr const char* LOG_MAGENTA = "\033[35m";
constexpr const char* LOG_CYAN    = "\033[36m";

// ---------------------------------------------------------------------------
// Source location + source manager
// ---------------------------------------------------------------------------
struct SourceLoc {
    int line = 0;            ///< 1-based line, 0 when unknown
    int column = 0;          ///< 1-based column (best-effort), 0 when unknown
    std::string file;        ///< file path (may be empty)
};

/// Holds the source text lines so the reporter can render contextual snippets.
class SourceManager {
public:
    void set_file(std::string f) { file_ = std::move(f); }
    void set_lines(std::vector<std::string> lines) { lines_ = std::move(lines); }

    [[nodiscard]] const std::string& file() const { return file_; }
    [[nodiscard]] const std::vector<std::string>& lines() const { return lines_; }
    [[nodiscard]] const std::string* line_text(int line) const; ///< 1-based, nullptr if absent

private:
    std::string file_;
    std::vector<std::string> lines_;
};

// ---------------------------------------------------------------------------
// Diagnostics: Issue + Reporter
// ---------------------------------------------------------------------------
struct Issue {
    enum class Severity { Error, Warning, Note, Hint };

    Severity severity = Severity::Note;
    SourceLoc loc;
    std::string code;          ///< short diagnostic code, e.g. "type-mismatch"
    std::string message;       ///< primary message (no location prefix)
    std::string suggestion;    ///< optional "help: ..." hint
    std::vector<std::string> notes; ///< related notes, e.g. related declarations
};

/// Emits issues to stderr with colored severity badges and source snippets.
class Reporter {
public:
    void set_color(bool enabled) { color_ = enabled; }
    void set_source(const SourceManager* src) { src_ = src; }
    void set_view_log(bool enabled) { view_log_ = enabled; }
    [[nodiscard]] bool view_log() const { return view_log_; }

    void emit(const Issue& issue);

    void error(const SourceLoc& loc, const std::string& msg,
               std::string code = {}, std::string suggestion = {});
    void warning(const SourceLoc& loc, const std::string& msg,
                 std::string code = {}, std::string suggestion = {});
    void note(const SourceLoc& loc, const std::string& msg);
    void hint(const SourceLoc& loc, const std::string& msg);

    /// Convenience overload taking a raw line number (kept for back-compat).
    void error(int line, const std::string& msg, const std::string& file = {});

    [[nodiscard]] const std::vector<Issue>& issues() const { return issues_; }
    [[nodiscard]] size_t error_count() const;
    [[nodiscard]] size_t warning_count() const;

private:
    std::vector<Issue> issues_;
    bool color_ = true;
    bool view_log_ = false;
    const SourceManager* src_ = nullptr;

    void render(const Issue& issue) const;
};

// ---------------------------------------------------------------------------
// Type system
// ---------------------------------------------------------------------------
/// A first-class type representation shared by inference and checking.
struct Type {
    enum class Kind {
        Unknown,    ///< could not be determined (treat permissively)
        Any,        ///< dynamic / unannotated
        None,       ///< nonetype
        Primitive,  ///< num, text, bool, vector, table ...
        Struct,     ///< a user-defined struct type
        Composite,  ///< parameterised type such as Maybe[T], Union[A,B], vec[T]
        Func,       ///< function/lambda type
    };

    Kind kind = Kind::Unknown;
    std::string name;                          ///< canonical name (e.g. "num", "Vec2", "Maybe")
    const lmx::StructDeclNode* struct_decl = nullptr;
    std::vector<std::shared_ptr<Type>> args;   ///< composite subtypes / func param types
    std::shared_ptr<Type> ret;                 ///< function return type
    bool optional = false;                     ///< whether the value may also be None

    [[nodiscard]] bool is_unknown() const { return kind == Kind::Unknown; }
    [[nodiscard]] bool is_any() const { return kind == Kind::Any; }
    [[nodiscard]] bool is_numeric() const;
    [[nodiscard]] bool is_text() const { return name == "text"; }
    [[nodiscard]] bool is_bool() const { return name == "bool"; }
    [[nodiscard]] bool is_vector() const { return name == "vector"; }
    [[nodiscard]] bool is_table() const { return name == "table"; }
    [[nodiscard]] bool is_struct() const { return kind == Kind::Struct; }
    [[nodiscard]] bool is_func() const { return kind == Kind::Func; }

    /// Human-readable rendering, e.g. ``Maybe[num]`` or ``Student``.
    [[nodiscard]] std::string to_string() const;
};

using TypePtr = std::shared_ptr<Type>;

// ---------------------------------------------------------------------------
// Type registry: builtins + user structs + aliasing + assignability
// ---------------------------------------------------------------------------
class TypeRegistry {
public:
    void register_struct(const lmx::StructDeclNode* s);
    void register_func(const lmx::FuncDeclNode* f);
    void register_lambda(const std::string& alias, const lmx::DoFuncDeclNode* f);

    [[nodiscard]] bool is_known_type_name(const std::string& name) const;
    [[nodiscard]] const lmx::StructDeclNode* find_struct(const std::string& name) const;
    [[nodiscard]] const lmx::FuncDeclNode* find_func(const std::string& name) const;

    /// Canonicalise an alias (``int`` -> ``num``, ``str`` -> ``text`` ...).
    [[nodiscard]] std::string canonicalize(const std::string& name) const;

    /// Build a Type from a canonical name. Struct names resolve to Struct kind.
    [[nodiscard]] TypePtr make_type(const std::string& canonical_name) const;

    /// Convert a parser TypeNode (possibly CompositeTypeNode) into a Type.
    [[nodiscard]] TypePtr from_type_node(const lmx::TypeNode* node) const;

    /// Build a function type from a function declaration.
    [[nodiscard]] TypePtr from_func_decl(const lmx::FuncDeclNode* f) const;

    /// Whether ``actual`` can be assigned to ``expected`` (with subtyping).
    [[nodiscard]] bool assignable(const TypePtr& expected, const TypePtr& actual) const;

    /// Suggest the closest known type name (Levenshtein). Empty if none close.
    [[nodiscard]] std::string suggest_type_name(const std::string& misspelled) const;

    /// Field lookup on a struct type; returns nullptr if absent.
    [[nodiscard]] const lmx::StructField* find_field(const TypePtr& struct_type,
                                                     const std::string& field_name) const;

    /// Suggest the closest field name on a struct (Levenshtein). Empty if none.
    [[nodiscard]] std::string suggest_field_name(const TypePtr& struct_type,
                                                 const std::string& misspelled) const;

private:
    std::unordered_map<std::string, const lmx::StructDeclNode*> structs_;
    std::unordered_map<std::string, const lmx::FuncDeclNode*> funcs_;
};

// ---------------------------------------------------------------------------
// Lexical scope
// ---------------------------------------------------------------------------
struct Binding {
    TypePtr type;        ///< inferred or declared type (used for inference)
    bool annotated = false; ///< true when the binding has an explicit `: Type`
};

class Scope {
public:
    explicit Scope(const Scope* parent = nullptr) : parent_(parent) {}

    void define(const std::string& name, TypePtr type, bool annotated = false) {
        bindings_[name] = Binding{std::move(type), annotated};
    }
    [[nodiscard]] TypePtr lookup(const std::string& name) const;
    /// True if the binding was introduced with an explicit type annotation.
    [[nodiscard]] bool is_annotated(const std::string& name) const;

private:
    std::unordered_map<std::string, Binding> bindings_;
    const Scope* parent_;
};

// ---------------------------------------------------------------------------
// Checker
// ---------------------------------------------------------------------------
class Checker {
public:
    Checker(Reporter& reporter, const std::string& file_path)
        : reporter_(reporter), file_path_(file_path) {}

    void set_view_log(bool enabled) { view_log_ = enabled; }

    int check(const lmx::ProgramASTNode* ast);

private:
    Reporter& reporter_;
    std::string file_path_;
    SourceManager src_;
    TypeRegistry registry_;
    bool view_log_ = false;

    // ---- symbol collection ----
    void collect_symbols(const lmx::ASTNode* node);

    // ---- inference ----
    TypePtr infer(const lmx::ExprNode* expr, const Scope& scope);
    TypePtr infer_first_return_type(const lmx::FuncDeclNode* f, const Scope& base);

    // ---- checking ----
    void check_program(const lmx::ProgramASTNode* prog);
    void check_node(const lmx::ASTNode* node, Scope& scope,
                    const TypePtr& current_ret);
    void check_func(const lmx::FuncDeclNode* func);
    void check_struct(const lmx::StructDeclNode* s);
    void check_var_decl(const lmx::VarDeclNode* var, Scope& scope,
                        const TypePtr& current_ret);
    void check_assign(const lmx::AssignNode* assign, Scope& scope,
                      const TypePtr& current_ret);
    void check_if(const lmx::IfStmtNode* ifs, Scope& scope,
                  const TypePtr& current_ret);
    void check_for(const lmx::ForLoopNode* loop, Scope& scope,
                   const TypePtr& current_ret);
    void check_while(const lmx::WhileStmtNode* wh, Scope& scope,
                     const TypePtr& current_ret);
    void check_loop(const lmx::LoopNode* lp, Scope& scope,
                    const TypePtr& current_ret);
    void check_return(const lmx::ReturnStmtNode* ret, Scope& scope,
                      const TypePtr& current_ret);
    void check_block(const lmx::BlockStmtNode* block, Scope& parent,
                     const TypePtr& current_ret);
    void check_module(const lmx::ModuleNode* mod, Scope& scope,
                      const TypePtr& current_ret);
    void check_struct_construct(const lmx::FuncCallExprNode* call,
                                const lmx::StructDeclNode* s,
                                const Scope& scope);
    void check_expr_node(const lmx::ExprNode* expr, const Scope& scope);

    // ---- diagnostics helpers ----
    SourceLoc loc(int line) const;
    void emit_type_mismatch(const SourceLoc& loc, const std::string& context,
                            const TypePtr& expected, const TypePtr& actual,
                            std::string suggestion = {});
};

// ---------------------------------------------------------------------------
// Verbose logging (used when --view-log is passed)
// ---------------------------------------------------------------------------
void view_log(const std::string& msg);
void view_log_info(const std::string& msg);
void view_log_match(const std::string& context, const TypePtr& t1,
                    const TypePtr& t2, int line);

/// Entry point used by the CLI.
int run_check(const std::string& file_path, bool view_log = false);

} // namespace olmcheck
