#include "test.hpp"
#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include "irgen/value_copy.hpp"
#include "irgen/opcode.hpp"
#include "irgen/bytecode_file.hpp"

#include <format>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace {

struct PoolSample {
    uint64_t step = 0;
    uint64_t iters = 0;
    size_t total = 0;
    size_t live = 0;
    size_t free = 0;
    size_t rss_kb = 0;
};

[[nodiscard]] size_t process_rss_kb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return counters.WorkingSetSize / 1024;
    }
#endif
    return 0;
}

[[nodiscard]] std::vector<PoolSample> benchmark_alloc_loop(
    const std::string& setup,
    const std::string& body,
    const int steps,
    const int reps_per_step
) {
    irgen::VM vm;

    lmx::ProgramASTNode* setup_ast = parse(setup);
    if (setup_ast == nullptr) {
        throw std::runtime_error("GC benchmark setup parse failed");
    }
    const auto setup_code = lm::irgen::Generator(setup_ast).gen();
    vm.code.insert(vm.code.end(), setup_code.begin(), setup_code.end());
    vm.run();
    delete setup_ast;

    lmx::ProgramASTNode* body_ast = parse(body);
    if (body_ast == nullptr) {
        throw std::runtime_error("GC benchmark body parse failed");
    }
    const auto body_code = lm::irgen::Generator(body_ast).gen();
    const size_t invoke_pc = vm.code.size();
    vm.code.insert(vm.code.end(), body_code.begin(), body_code.end());
    delete body_ast;

    std::vector<PoolSample> samples;
    samples.reserve(static_cast<size_t>(steps) + 1);

    uint64_t total_iters = 0;
    for (int step = 0; step <= steps; ++step) {
        samples.push_back({
            .step = static_cast<uint64_t>(step),
            .iters = total_iters,
            .total = vm.cell_pool.totalCells(),
            .live = vm.cell_pool.liveCells(),
            .free = vm.cell_pool.freeCells(),
            .rss_kb = process_rss_kb(),
        });

        if (step == steps) {
            break;
        }

        for (int rep = 0; rep < reps_per_step; ++rep) {
            vm.pc = invoke_pc;
            vm.run();
            ++total_iters;
        }
    }

    return samples;
}

void print_trend_block(
    const std::string& title,
    const std::string& body_hint,
    const int reps_per_step,
    const std::vector<PoolSample>& samples
) {
    const bool show_rss = samples.empty() ? false : samples.front().rss_kb != 0;

    std::cout << '\n' << title << '\n';
    std::cout << "  body: " << body_hint << "  |  +" << reps_per_step << " reps/step\n";

    if (show_rss) {
        std::cout << std::format(
            "  {:>4} {:>8} {:>8} {:>7} {:>7} {:>7} {:>8}\n",
            "step", "iter", "total", "live", "free", "dlive", "rss(KB)"
        );
    } else {
        std::cout << std::format(
            "  {:>4} {:>8} {:>8} {:>7} {:>7} {:>7}\n",
            "step", "iter", "total", "live", "free", "dlive"
        );
    }

    const size_t live0 = samples.front().live;
    for (const PoolSample& row : samples) {
        const long long delta_live = static_cast<long long>(row.live) - static_cast<long long>(live0);
        const std::string delta_text = row.step == 0 ? "   -" : std::format("{:+4}", delta_live);

        if (show_rss) {
            std::cout << std::format(
                "  {:4} {:8} {:8} {:7} {:7} {:7} {:8}\n",
                row.step,
                row.iters,
                row.total,
                row.live,
                row.free,
                delta_text,
                row.rss_kb
            );
        } else {
            std::cout << std::format(
                "  {:4} {:8} {:8} {:7} {:7} {:7}\n",
                row.step,
                row.iters,
                row.total,
                row.live,
                row.free,
                delta_text
            );
        }
    }
}

void print_legend() {
    std::cout << "\n  total = pool cells  |  live = reachable  |  free = recycled\n";
    std::cout << "  dlive = live - live(step 0)  |  flat dlive + stable total => GC keeping pace\n";
}

} // namespace

void test_gc_memory_trend() {
    constexpr int k_steps = 12;
    constexpr int k_reps_per_step = 250;

    const std::string setup = "use std.math.{range}";

    const auto no_gc = benchmark_alloc_loop(
        setup,
        "range(1000)",
        k_steps,
        k_reps_per_step
    );

    const auto with_gc = benchmark_alloc_loop(
        setup,
        "range(1000)\ngc()",
        k_steps,
        k_reps_per_step
    );

    std::cout << "=== OpenLamina GC memory trend ===\n";
    std::cout << "simulates loop { std.math.range(1000) } via repeated VM invocations\n";

    print_trend_block(
        "[A] auto-GC only (no explicit gc())",
        "range(1000)",
        k_reps_per_step,
        no_gc
    );

    print_trend_block(
        "[B] explicit gc() each iteration",
        "range(1000); gc()",
        k_reps_per_step,
        with_gc
    );

    print_legend();
    std::cout << std::flush;
}

void test_bytecode_optimizer() {
    lm::irgen::bytecode_optimize_enabled = false;

    const std::string src = "10 + 20 + 30";
    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);

    const std::vector<irgen::Opcode> raw = lm::irgen::Generator(ast).gen();
    const size_t raw_ops = raw.size();

    std::vector<irgen::Opcode> optimized = raw;
    const lm::irgen::OptimizeReport report = lm::irgen::optimize_bytecode(optimized);

    std::cout << "\n=== bytecode optimizer ===\n";
    std::cout << std::format(
        "  ops: {} -> {}  |  constant_folds={}  jumps_threaded={}  dead_removed={}\n",
        report.ops_before,
        report.ops_after,
        report.constant_folds,
        report.jumps_threaded,
        report.dead_ops_removed
    );

    ASSERT(report.constant_folds > 0);
    ASSERT(optimized.size() < raw_ops);

    const auto run_code = [](const std::vector<irgen::Opcode>& code) -> irgen::Value {
        irgen::VM vm(code);
        vm.run();
        ASSERT(!vm.op_stack.empty());
        return vm.op_stack.top().deref();
    };

    const irgen::Value raw_result = run_code(raw);
    const irgen::Value opt_result = run_code(optimized);
    ASSERT(raw_result.isNumber());
    ASSERT(opt_result.isNumber());
    ASSERT(raw_result.asNumber() == opt_result.asNumber());
    ASSERT(raw_result.asNumber() == irgen::Value(static_cast<int64_t>(60)).asNumber());

    delete ast;
}

void test_bytecode_lmc_roundtrip() {
    const std::string src = R"(
func add(a, b) {
    return a + b
}
let x = 10 + 20
let y = add(x, 5)
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);

    const lm::irgen::CompiledModule module = lm::irgen::compile_ast_optimized(ast);
    delete ast;

    ASSERT(module.optimized);
    ASSERT(!module.code.empty());

    const std::string path = "test_roundtrip.lmc";
    lm::irgen::save_lmc(path, module);
    lm::irgen::CompiledModule loaded = lm::irgen::load_lmc(path);
    std::filesystem::remove(path);

    ASSERT(loaded.code.size() == module.code.size());
    ASSERT(lm::irgen::run_compiled_module(loaded, [](irgen::VM&) { return true; }));
}

void test_type_convert_sugar() {
    const std::string src = R"(
struct A { var a }
struct C { var c }
A.__convert__.__dispatch__.append(do(self, x: C) { return A(x.c) })
A.(C(1))
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);

    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& result = vm.op_stack.top();
        ASSERT(result.isStruct());
        const auto inst = result.deref().asStruct();
        ASSERT(inst->type->name == "A");
        ASSERT(inst->slots[0].isNumber());
        ASSERT(inst->slots[0].asNumber() == irgen::Value(static_cast<int64_t>(1)).asNumber());
        return true;
    }));

    delete ast;
}

void test_exceptions_and_try_catch() {
    const std::string src = R"(
struct Counter {
    var n
    func __next__(self) {
        if (self.n > 0) {
            self.n = self.n - 1
            return self.n + 1
        }
        throw StopIteration()
    }
}

let out = vec[]
for (i in Counter(3)) {
    out.append(i)
}

let hit = 0
let else_hit = 0
try {
    throw ValueError("bad")
} catch (e: ValueError) {
    hit = 1
} else {
    else_hit = 1
}

let ok = 0
try {
    throw TypeError("t")
} catch (...) {
    ok = 1
}

let any_msg = ""
try {
    throw RuntimeError("from-any")
} catch (e) {
    any_msg = e.message
}

let bare = 0
try {
    throw ValueError("bare")
} catch (err) {
    bare = 1
}

let summary = vec[]
summary.append(out.len())
summary.append(hit)
summary.append(else_hit)
summary.append(ok)
summary.append(any_msg)
summary.append(bare)
summary
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& result = vm.op_stack.top();
        ASSERT(result.isVector());
        const auto& slots = result.deref().asVector();
        ASSERT(slots.size() == 6);
        ASSERT(slots[0]->deref().asNumber() == irgen::Value(static_cast<int64_t>(3)).asNumber());
        ASSERT(slots[1]->deref().asNumber() == irgen::Value(static_cast<int64_t>(1)).asNumber());
        ASSERT(slots[2]->deref().asNumber() == irgen::Value(static_cast<int64_t>(0)).asNumber());
        ASSERT(slots[3]->deref().asNumber() == irgen::Value(static_cast<int64_t>(1)).asNumber());
        ASSERT(slots[4]->deref().isString());
        ASSERT(slots[4]->deref().asString() == "from-any");
        ASSERT(slots[5]->deref().asNumber() == irgen::Value(static_cast<int64_t>(1)).asNumber());
        return true;
    }));
    delete ast;

    bool throw_rejected = false;
    try {
        lmx::ProgramASTNode* bad_ast = parse("throw 1");
        ASSERT(bad_ast != nullptr);
        lm::irgen::execute(bad_ast, [](irgen::VM&) { return true; });
        delete bad_ast;
    } catch (const RuntimeError& e) {
        throw_rejected = std::string(e.what()).find("can only throw exception") != std::string::npos;
    }
    ASSERT(throw_rejected);
}

void test_friend_func_dispatch() {
    const std::string src = R"(
friend func addOne(x: num) {
    return x + 1
}

addOne.__dispatch__.append(
    do(x: text) {
        return x + "!"
    }
)

friend func noMain

noMain.__dispatch__.append(
    do(k: num) {
        return k + 10
    }
)

noMain.__dispatch__.append(
    do(k: num) {
        return k + 100
    }
)

let summary = vec[]
summary.append(addOne(5))
summary.append(addOne("hi"))
summary.append(noMain(7))
summary
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value result = irgen::detach_value(vm.op_stack.top());
        ASSERT(result.isVector());
        const auto& slots = result.asVector();
        ASSERT(slots.size() == 3);
        ASSERT(slots[0] != nullptr);
        ASSERT(slots[0]->deref().asNumber() == irgen::Value(static_cast<int64_t>(6)).asNumber());
        ASSERT(slots[1] != nullptr);
        ASSERT(slots[1]->deref().isString());
        ASSERT(slots[1]->deref().asString() == "hi!");
        ASSERT(slots[2] != nullptr);
        ASSERT(slots[2]->deref().asNumber() == irgen::Value(static_cast<int64_t>(107)).asNumber());
        return true;
    }));
    std::cerr << "[test] friend_func_dispatch execute done\n" << std::flush;
    delete ast;
    std::cerr << "[test] friend_func_dispatch ast deleted\n" << std::flush;

    bool missing_impl = false;
    try {
        lmx::ProgramASTNode* bad_ast = parse("friend func empty\nempty(1)");
        ASSERT(bad_ast != nullptr);
        lm::irgen::execute(bad_ast, [](irgen::VM&) { return true; });
        delete bad_ast;
    } catch (const RuntimeError& e) {
        missing_impl = std::string(e.what()).find("no matching __dispatch__") != std::string::npos;
    }
    ASSERT(missing_impl);
}

void test_iter_stopiteration_edges() {
    const std::string src = R"(
struct Counter {
    var n
    func __next__(self) {
        if (self.n > 0) {
            self.n = self.n - 1
            return self.n + 1
        }
        throw StopIteration()
    }
}

let via_iter = vec[]
let it = iter(Counter(2))
via_iter.append(next(it))
via_iter.append(next(it))

let outer = 0
try {
    for (x in Counter(1)) {
        outer = outer + x
    }
} catch (e: StopIteration) {
    outer = -1
}

let base_hit = 0
try {
    throw ValueError("sub")
} catch (e: Exception) {
    base_hit = 1
}

let out = vec[]
out.append(via_iter.len())
out.append(via_iter[0])
out.append(via_iter[1])
out.append(outer)
out.append(base_hit)
out
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& result = vm.op_stack.top();
        ASSERT(result.isVector());
        const auto& slots = result.deref().asVector();
        ASSERT(slots.size() == 5);
        ASSERT(slots[0]->deref().asNumber() == irgen::Value(static_cast<int64_t>(2)).asNumber());
        ASSERT(slots[1]->deref().asNumber() == irgen::Value(static_cast<int64_t>(2)).asNumber());
        ASSERT(slots[2]->deref().asNumber() == irgen::Value(static_cast<int64_t>(1)).asNumber());
        ASSERT(slots[3]->deref().asNumber() == irgen::Value(static_cast<int64_t>(1)).asNumber());
        ASSERT(slots[4]->deref().asNumber() == irgen::Value(static_cast<int64_t>(1)).asNumber());
        return true;
    }));
    delete ast;
}

void test_macro_system() {
    const std::string src = R"(
macro sq(x) {
    return quote(ex) with (x) {
        var ex = eval(x)
        ex * ex
    }
}

n = 6
sq{n}
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& result = vm.op_stack.top().deref();
        ASSERT(result.isNumber());
        ASSERT(result.asNumber() == irgen::Value(static_cast<int64_t>(36)).asNumber());
        return true;
    }));
    delete ast;
}

void test_macro_identity() {
    const std::string src = R"(
macro identity(x) {
    return x
}
n = 6
identity{n}
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& result = vm.op_stack.top().deref();
        ASSERT(result.isNumber());
        ASSERT(result.asNumber() == irgen::Value(static_cast<int64_t>(6)).asNumber());
        return true;
    }));
    delete ast;
}

void test_macro_variadic_log() {
    const std::string src = R"(
macro LOG(*msg) {
    vals = []
    for (ast in msg) {
        vals.append(eval(ast))
    }
    print("[LOG]", *vals)
}

LOG{"a", ":", 5}
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        (void)vm;
        return true;
    }));
    delete ast;
}

void test_macro_it_text() {
    const std::string src = R"(
macro LOG(*msg) {
    vals = []
    for (ast in msg) {
        vals.append(eval(ast))
    }
    print("[LOG]", *vals)
}

macro it(x) {
    LOG{text.(x), ":", x}
}

a = 5
it{a}
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        (void)vm;
        return true;
    }));
    delete ast;
}

void test_macro_no_return() {
    const std::string src = R"(
macro noop() {
}
noop{}
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        ASSERT(vm.op_stack.top().deref().isNone());
        return true;
    }));
    delete ast;
}

namespace {

[[nodiscard]] bool execute_throws_substr(const std::string& src, const std::string& needle) {
    lmx::ProgramASTNode* ast = parse(src);
    if (ast == nullptr) {
        return false;
    }
    bool matched = false;
    try {
        lm::irgen::execute(ast, [](irgen::VM&) { return true; });
    } catch (const RuntimeError& e) {
        matched = std::string(e.what()).find(needle) != std::string::npos;
    }
    delete ast;
    return matched;
}

} // namespace

// eval(MacroCallExpr/FuncCallExpr) inside LOG while outer macro runs.
void test_macro_bug_eval_call_in_log() {
    const std::string setup = R"(
macro LOG(*msg) {
    vals = []
    for (ast in msg) {
        vals.append(eval(ast))
    }
    print("[LOG]", *vals)
}

macro it(x) {
    LOG{text.(x), ":", x}
}

macro square(x) {
    return quote(ex) with (x) {
        var ex = eval(x)
        ex * ex
    }
}

func double_it(x) {
    return x * x
}
)";

    lmx::ProgramASTNode* ast1 = parse(setup + R"(
a = 5
it{square{a}}
)");
    ASSERT(ast1 != nullptr);
    ASSERT(lm::irgen::execute(ast1, [](irgen::VM& vm) {
        (void)vm;
        return true;
    }));
    delete ast1;

    lmx::ProgramASTNode* ast2 = parse(setup + R"(
k = 3
it{double_it(k)}
)");
    ASSERT(ast2 != nullptr);
    ASSERT(lm::irgen::execute(ast2, [](irgen::VM& vm) {
        (void)vm;
        return true;
    }));
    delete ast2;
}

// nested macro{} in macro body during AST materialization.
void test_macro_bug_nested_macro_compose() {
    const std::string src = R"(
macro square(x) {
    return quote(ex) with (x) {
        var ex = eval(x)
        ex * ex
    }
}

macro pow4_bad(x) {
    return square{square{x}}
}

pow4_bad{2}
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& result = vm.op_stack.top().deref();
        ASSERT(result.isNumber());
        ASSERT(result.asNumber() == irgen::Value(static_cast<int64_t>(16)).asNumber());
        return true;
    }));
    delete ast;
}

void test_macro_nested_splat() {
    const std::string src = R"(
macro LOG(*msg) {
    vals = []
    for (ast in msg) {
        vals.append(eval(ast))
    }
    print("[LOG]", *vals)
}

macro bundle(*items) {
    return LOG{*items}
}

bundle{"x", ":", 2}
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        (void)vm;
        return true;
    }));
    delete ast;
}

void test_macro_ast_struct_match() {
    const std::string src = R"(
macro inspect(x) {
    return quote(ex) with (x) {
        match (ast_struct(x)) {
            case AstVarRef { name } {
                print("var", name)
            }
            case AstNumber { value } {
                print("num", value)
            }
        } else {
            print("other")
        }
    }
}

inspect{a}
a = 5
inspect{7}
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        (void)vm;
        return true;
    }));
    delete ast;
}

void test_type_handle() {
    const std::string src = R"(
print(type(42) == num)
print(type("x") == text)
print(type(vec[]) == vector)

macro inspect(x) {
    return quote(ex) with (x) {
        print(type(x) == AST)
        print(type(ast_struct(x)) == AstVarRef)
    }
}
sym = 0
inspect{sym}
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        (void)vm;
        return true;
    }));
    delete ast;
}

void test_quote_syntax_forms() {
    const std::string src = R"(
macro double_it(x) {
    return quote(a) with (x) {
        var a = eval(x)
        a + a
    }
}

macro shadow_demo() {
    return quote(a) {
        var a = 99
        a + 1
    }
}

macro inc(x) {
    return quote with (x) {
        eval(x) + 1
    }
}

macro forty_two() {
    return quote {
        42
    }
}

print(double_it{5})
print(shadow_demo{})
print(inc{10})
print(forty_two{})
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        (void)vm;
        return true;
    }));
    delete ast;
}

void test_container_methods_and_format() {
    const std::string src = R"(
use std.format.{format, join}
let v = vec[10, 20, 30]
v.set(1, 99)
v.insert(0, 5)
let a = v.get(2)
let d = {"x": 1}
d.put("y", 2)
d.set("x", 9)
let got = d.get("x")
let items = d.items().len()
let fmt = format("{} + {} = {}", 1, 2, 3)
let joined = join("-", vec["a", "b"])
struct Box { var n func fail(self) { throw ValueError("boom") } }
let caught = 0
let after_fail = 0
try {
    Box(1).fail()
    after_fail = 1
} catch (e: ValueError) {
    caught = 1
}
print(a, got, items, fmt, joined, caught, after_fail)
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        (void)vm;
        return true;
    }));
    delete ast;
}

void test_try_catch_skips_after_struct_throw() {
    const std::string src = R"(
struct Lib {
    func fail(self) { throw ValueError("err") }
}
let lib = Lib()
let flag = 0
try {
    lib.fail()
    flag = 1
} catch (e: ValueError) {
    flag = 2
}
flag
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& result = vm.op_stack.top();
        ASSERT(result.isNumber());
        ASSERT(result.asNumber() == irgen::Value(static_cast<int64_t>(2)).asNumber());
        return true;
    }));
    delete ast;
}

void test_elif_and_or_not() {
    const std::string src = R"(
let side = vec[]

func bump(tag) {
    side.append(tag)
}

let branch = 0
if (false) {
    branch = 1
} elif (true) {
    branch = 2
} else {
    branch = 3
}

let and_val = 0 and bump("and_lhs") or bump("or_lhs")
let sc = vec[]
if (1 and bump("sc_and") and 0) {
    sc.append(1)
} else {
    sc.append(2)
}

let not_ok = not ""
let not_fail = not 0

let summary = vec[]
summary.append(branch)
summary.append(and_val)
summary.append(side.len())
summary.append(sc[0])
summary.append(not_ok)
summary.append(not_fail)
summary
)";

    lmx::ProgramASTNode* ast = parse(src);
    ASSERT(ast != nullptr);
    ASSERT(lm::irgen::execute(ast, [](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& result = vm.op_stack.top();
        ASSERT(result.isVector());
        const auto& slots = result.deref().asVector();
        ASSERT(slots.size() == 6);
        ASSERT(slots[0]->deref().asNumber() == irgen::Value(static_cast<int64_t>(2)).asNumber());
        ASSERT(slots[1]->deref().isNone());
        ASSERT(slots[2]->deref().asNumber() == irgen::Value(static_cast<int64_t>(2)).asNumber());
        ASSERT(slots[3]->deref().asNumber() == irgen::Value(static_cast<int64_t>(2)).asNumber());
        ASSERT(slots[4]->deref().isBool());
        ASSERT(slots[4]->deref().asBool());
        ASSERT(slots[5]->deref().isBool());
        ASSERT(slots[5]->deref().asBool());
        return true;
    }));
    delete ast;
}
