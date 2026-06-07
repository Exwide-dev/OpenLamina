#include "repl.hpp"
#include "../lexer/utf8.hpp"
#include "irgen/generator.hpp"
#include "../tools/error.hpp"

void repl::REPL::reset_state() {
    brace_stack = {};
    pending_input.clear();
    in_string = false;
    string_delimiter = '"';
}

void repl::REPL::update_state(const std::string& line) {
    bool prev_char_is_whitespace = true;
    std::string prev_token;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (in_string) {
            if (c == '\\' && i + 1 < line.size()) {
                i++;
                continue;
            }
            if (c == string_delimiter) {
                in_string = false;
            }
            continue;
        }

        if (c == '"') {
            in_string = true;
            string_delimiter = '"';
            continue;
        }

        if (std::isspace(c)) {
            prev_char_is_whitespace = true;
            continue;
        }

        std::string token;
        if (lmx::utf8::identifier_start_at(line, i)) {
            size_t j = i;
            while (j < line.size() && lmx::utf8::identifier_continue_at(line, j)) {
                j += lmx::utf8::codepoint_length(line, j);
            }
            token = line.substr(i, j - i);
            i = j - 1;
        } else {
            token = line.substr(i, lmx::utf8::codepoint_length(line, i));
        }

        if (token == "(") {
            brace_stack.push(BraceType::PAREN);
        } else if (token == ")") {
            if (!brace_stack.empty() && brace_stack.top() == BraceType::PAREN) {
                brace_stack.pop();
            }
        } else if (token == "[") {
            brace_stack.push(BraceType::BRACKET);
        } else if (token == "]") {
            if (!brace_stack.empty() && brace_stack.top() == BraceType::BRACKET) {
                brace_stack.pop();
            }
        } else if (token == "{") {
            bool is_func_body = false;
            if (!prev_token.empty()) {
                static const std::string func_keywords[] = {
                    "func",
                    "do",
                    "if",
                    "else",
                    "loop",
                    "while"
                };
                for (const auto& kw : func_keywords) {
                    if (prev_token == kw) {
                        is_func_body = true;
                        break;
                    }
                }
            }

            if (is_func_body || (!brace_stack.empty() && brace_stack.top() == BraceType::BRACE_FUNC)) {
                brace_stack.push(BraceType::BRACE_FUNC);
            } else {
                brace_stack.push(BraceType::BRACE_DICT);
            }
        } else if (token == "}") {
            if (!brace_stack.empty()) {
                if (brace_stack.top() == BraceType::BRACE_FUNC ||
                    brace_stack.top() == BraceType::BRACE_DICT) {
                    brace_stack.pop();
                }
            }
        }

        prev_token = token;
        prev_char_is_whitespace = false;
    }
}

bool repl::REPL::needs_more_input() const {
    return !brace_stack.empty();
}

repl::REPL::ExecResult repl::REPL::exec_input(const std::function<std::string()>& input_func) {
    const std::string line = input_func();
    if (line.empty()) {
        if (!pending_input.empty()) {
            reset_state();
        }
        return {false, false};
    }

    update_state(line);

    if (!pending_input.empty()) {
        pending_input += "\n";
    }
    pending_input += line;

    if (needs_more_input()) {
        return {true, true};
    }

    const std::string input = pending_input;
    reset_state();

    if (input.empty()) return {false, false};

    std::vector<lmx::ASTNode*> stmts = parse_session.parse_chunk(input);
    if (stmts.empty()) {
        return {true, false};
    }

    auto* got_ast = new lmx::ProgramASTNode(std::move(stmts));
    auto code = lm::irgen::Generator(got_ast).gen();
    vm.code.insert(vm.code.end(), code.begin(), code.end());
    vm.run();

    vm.call_stack.clear();
    delete got_ast;
    return {true, false};
}