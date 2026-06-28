#pragma once

#include <vector>
#include <string>

constexpr std::string RED_BOLD = "\033[1;31m";
constexpr std::string GREEN = "\033[0;32m";
constexpr std::string YELLOW = "\033[0;33m";
constexpr std::string BLUE = "\033[0;34m";
constexpr std::string MAGENTA = "\033[0;35m";
constexpr std::string CYAN = "\033[0;36m";
constexpr std::string RESET = "\033[0m";

namespace cli {
/**
     * @brief 命令行参数解析结果
     */
struct Args {
    bool show_help = false;
    bool show_version = false;
    bool optimize = true;
    bool compile = false;
    std::string file_path;
    std::string output_path;
    std::vector<std::string> script_args;
};

/**
     * @brief 解析命令行参数
     * @param argc 参数个数
     * @param argv 参数数组
     * @return 解析结果
     */
Args parse_args(int argc, char* argv[]);

/** @brief 根据 -O / -O0 设置全局字节码优化开关（默认开启） */
void apply_runtime_flags(const Args& args);

/**
     * @brief 运行脚本文件
     */
int run_file(const std::string& file_path, const std::vector<std::string>& args);

/**
     * @brief 运行已编译的 .lmc 字节码文件
     */
int run_bytecode_file(const std::string& file_path);

/**
     * @brief 将 .lm 源文件编译为 .lmc 字节码
     */
int compile_file(const std::string& file_path, const std::string& output_path);

/**
     * @brief 启动 REPL 交互环境
     */
int run_repl();

/**
     * @brief 显示帮助信息
     */
void show_help();

/**
     * @brief 显示版本信息
     */
void show_version();
} // namespace cli