#pragma once

#include <vector>
#include <string>

namespace cli {

    /**
     * @brief 命令行参数解析结果
     */
    struct Args {
        bool show_help = false;
        bool show_version = false;
        std::string file_path;
        std::vector<std::string> script_args;
    };

    /**
     * @brief 解析命令行参数
     * @param argc 参数个数
     * @param argv 参数数组
     * @return 解析结果
     */
    Args parse_args(int argc, char* argv[]);

    /**
     * @brief 运行脚本文件
     * @param file_path 文件路径
     * @param args 脚本参数
     * @return 退出码
     */
    int run_file(const std::string& file_path, const std::vector<std::string>& args);

    /**
     * @brief 启动 REPL 交互环境
     * @return 退出码
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