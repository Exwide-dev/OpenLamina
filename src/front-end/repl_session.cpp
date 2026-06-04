#include "repl_session.hpp"

#include "../../tools/utf8_io.hpp"
#include "../../tools/error.hpp"
#include "../../tools/debug.hpp"

namespace lmx {

ReplSession::ReplSession(std::string filename)
    : filename_(std::move(filename)),
      lexer_(filename_),
      parser_(filename_) {}

void ReplSession::reset() {
    lexer_ = Lexer(filename_);
    parser_ = Parser(filename_);
    cumulative_source_.clear();
}

std::vector<ASTNode*> ReplSession::parse_chunk(const std::string& source) {
    const std::string utf8 = lm::utf8_io::normalize_to_utf8(source);
    if (utf8.empty()) {
        return {};
    }

    std::string chunk = utf8;
    if (!cumulative_source_.empty()) {
        if (!cumulative_source_.ends_with('\n') && (chunk.empty() || chunk.front() != '\n')) {
            chunk.insert(chunk.begin(), '\n');
        }
    }

    cumulative_source_ += chunk;

    lexer_.add_input(chunk);
    std::vector<Token> tokens = lexer_.lex_rest();

    for (const auto& tok : tokens) {
        if (tok.type == TokenType::END && !tok.value.empty()) {
            throw SyntaxError(
                "Lexer error: unclosed string literal at line " +
                std::to_string(tok.line) + ", column " + std::to_string(tok.column));
        }
    }

    parser_.append_tokens(std::move(tokens), chunk);
    parser_.set_source_lines(cumulative_source_);
    return parser_.parse_rest();
}

} // namespace lmx
