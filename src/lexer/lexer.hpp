#pragma once

#include <string>
#include <utility>
#include <vector>
#include <regex>
#include <cctype>

namespace lmx {
/**
 * @enum TokenType
 * @brief 词法分析器识别的所有 token 类型
 */
enum class TokenType {
    END,            ///< 文件结束标记
    IDENTIFIER,     ///< 标识符
    NUM_LITERAL,    ///< 数字字面量
    STRING_LITERAL, ///< 字符串字面量

    KW_LET,      ///< let 关键字
    KW_FUNC,     ///< func 关键字
    KW_DO,       ///< do 关键字
    KW_RETURN,   ///< return 关键字
    KW_IF,       ///< if 关键字
    KW_ELSE,     ///< else 关键字
    KW_LOOP,     ///< loop 关键字
    KW_WHILE,    ///< while 关键字
    KW_BREAK,    ///< break 关键字
    KW_CONTINUE, ///< continue 关键字
    KW_IMPORT,   ///< import 关键字
    KW_USE,      ///< use 关键字
    KW_AS,       ///< as 关键字
    KW_VEC,      ///< vec 关键字
    KW_CONST,    ///< const 关键字
    KW_VAR,      ///< var 关键字
    KW_INTERN,   ///< intern 关键字
    KW_EXPORT,   ///< export 关键字
    KW_WITH,     ///< with 关键字
    KW_MAKE,     ///< make 关键字
    KW_FOR,      ///< for 关键字
    KW_IN,       ///< in 关键字
    KW_STRUCT,   ///< struct 关键字
    KW_TYPED,    ///< typed 关键字（启用 struct 字段类型检查）

    OPER_PLUS,  ///< 加法运算符 +
    OPER_MINUS, ///< 减法运算符 -
    OPER_MUL,   ///< 乘法运算符 *
    OPER_DIV,   ///< 除法运算符 /
    OPER_NOT,   ///< 逻辑非运算符 !
    OPER_EQ,    ///< 相等运算符 ==
    OPER_NE,    ///< 不等运算符 !=
    OPER_LT,    ///< 小于运算符 <
    OPER_GT,    ///< 大于运算符 >
    OPER_LE,    ///< 小于等于运算符 <=
    OPER_GE,    ///< 大于等于运算符 >=
    OPER_COMMA, ///< 逗号 ,
    OPER_DOT,   ///< 点号 .
    OPER_COLON, ///< 冒号 :
    ASSIGN,     ///< 赋值运算符 =

    LPAREN,   ///< 左圆括号 (
    RPAREN,   ///< 右圆括号 )
    LBRACE,   ///< 左花括号 {
    RBRACE,   ///< 右花括号 }
    LBRACKET, ///< 左方括号 [
    RBRACKET, ///< 右方括号 ]
    NEWLINE,  ///< 换行符
    MISMATCH  ///< 无法识别的字符
};

/**
 * @struct Token
 * @brief 词法单元结构，存储 token 的类型、值和位置信息
 */
struct Token {
    TokenType type;    ///< token 类型
    std::string value; ///< token 的字符串值
    int line;          ///< 所在行号
    int column;        ///< 所在列号

    /**
     * @brief 构造函数
     * @param t token 类型
     * @param v token 值
     * @param l 行号
     * @param c 列号
     */
    Token(const TokenType t, std::string v, const int l, const int c)
        : type(t), value(std::move(v)), line(l), column(c) {
    }
};

/**
 * @struct TokenPattern
 * @brief token 匹配模式，用于正则表达式匹配
 */
struct TokenPattern {
    std::regex regex; ///< 正则表达式
    TokenType type;   ///< 匹配到的 token 类型

    /**
     * @brief 构造函数
     * @param pattern 正则表达式字符串
     * @param t token 类型
     */
    TokenPattern(const std::string& pattern, TokenType t) : regex(pattern), type(t) {
    }
};

/**
 * @struct LexError
 * @brief 词法分析错误结构
 */
struct LexError {
    std::string message; ///< 错误信息
    int line;            ///< 错误所在行号
    int column;          ///< 错误所在列号

    /**
     * @brief 构造函数
     * @param msg 错误信息
     * @param ln 行号
     * @param col 列号
     */
    LexError(std::string msg, int ln, int col)
        : message(std::move(msg)), line(ln), column(col) {
    }
};

/**
 * @class Lexer
 * @brief 词法分析器类，负责将源代码字符串转换为 token 序列
 */
class Lexer {
public:
    /**
     * @brief 构造函数
     * @param filename 源文件名，用于错误报告
     */
    explicit Lexer(std::string filename = "");

    /**
     * @brief 添加输入源代码
     * @param source 源代码字符串
     */
    void add_input(const std::string& source);

    /**
     * @brief 执行词法分析，生成 token 序列
     * @return token 向量
     */
    std::vector<Token> tokenize();

    /**
     * @brief 继续词法分析剩余输入
     * @return token 向量
     */
    std::vector<Token> lex_rest();

    /**
     * @brief 获取源文件名
     * @return 文件名
     */
    [[nodiscard]] const std::string& get_filename() const { return filename; }

    /**
     * @brief 获取源代码行列表
     * @return 源代码行向量
     */
    [[nodiscard]] const std::vector<std::string>& get_source_lines() const { return source_lines; }

    /**
     * @brief 检查是否存在词法错误
     * @return 如果有错误返回 true
     */
    [[nodiscard]] bool has_errors() const { return !errors.empty(); }

    /**
     * @brief 获取词法错误列表
     * @return 错误向量
     */
    [[nodiscard]] const std::vector<LexError>& get_errors() const { return errors; }

    /**
     * @brief 清除所有错误
     */
    void clear_errors() { errors.clear(); }

private:
    std::string filename;                  ///< 源文件名
    std::string full_source;               ///< 完整源代码
    std::vector<std::string> source_lines; ///< 源代码行列表
    std::vector<LexError> errors;          ///< 错误列表

    size_t pos = 0; ///< 当前解析位置（字节）
    int line = 1;   ///< 当前行号
    int column = 1; ///< 当前列号（按 Unicode 码点计）

    /**
     * @brief 词法分析器快照，用于预测/最长匹配时的状态回退
     */
    struct LexerState {
        size_t pos = 0;
        int line = 1;
        int column = 1;
    };

    [[nodiscard]] LexerState save_state() const {
        return {pos, line, column};
    }

    void restore_state(const LexerState& state) {
        pos = state.pos;
        line = state.line;
        column = state.column;
    }

    /**
     * @brief 获取 token 类型的名称字符串
     * @param type token 类型
     * @return 类型名称
     */
    static std::string getTokenTypeName(TokenType type);

    /**
     * @brief 初始化 token 匹配模式
     * @return 模式向量
     */
    static std::vector<TokenPattern> initPatterns();

    /**
     * @brief 查看当前字节（不消费）
     */
    [[nodiscard]] char peekChar() const;

    /**
     * @brief 消费当前字节
     */
    char consumeChar();

    /**
     * @brief 消费一个 UTF-8 码点（无效序列时退化为单字节）
     * @return 消费的字节数
     */
    size_t consumeCodepoint();

    /**
     * @brief 当前位置是否为标识符起始
     */
    [[nodiscard]] bool identifierStartsHere() const;

    /**
     * @brief 当前位置是否为标识符继续字符
     */
    [[nodiscard]] bool identifierContinuesHere() const;

    /**
     * @brief 跳过 UTF-8 BOM（若存在）
     */
    void skipBom();

    /**
     * @brief 检查是否到达输入末尾
     * @return 如果到达末尾返回 true
     */
    [[nodiscard]] bool isAtEnd() const;

    /**
     * @brief 解析标识符或关键字
     * @return 解析出的 token
     */
    Token parseIdentifierOrKeyword();

    /**
     * @brief 解析字符串字面量
     * @return 解析出的 token
     */
    Token parseString();

    /**
     * @brief 解析换行符
     * @return 解析出的 token
     */
    Token parseNewline();

    /**
     * @brief 尝试匹配正则模式
     * @return 匹配到的 token
     */
    Token tryMatchPatterns();

    /**
     * @brief 添加词法错误
     * @param message 错误信息
     */
    void add_error(const std::string& message);

    /**
     * @brief 判断是否为数字字符
     * @param c 字符
     * @return 如果是数字返回 true
     */
    [[nodiscard]] bool isDigit(const char c) const {
        const auto uc = static_cast<unsigned char>(c);
        return uc >= '0' && uc <= '9';
    }

    /**
     * @brief 判断是否为空白字符
     * @param c 字符
     * @return 如果是空白返回 true
     */
    [[nodiscard]] bool isWhitespace(const char c) const {
        const auto uc = static_cast<unsigned char>(c);
        return (uc == ' ' || uc == '\t' || uc == '\r');
    }
};
} // namespace lmx
