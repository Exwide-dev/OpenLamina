# OpenLamina

OpenLamina 被设计为 Lamina 语言的超集的实现，采用 Flex 和 Bison 构建词法和语法分析器，支持交互式 REPL 环境。

本项目使用了 Lamina-dev 的 LAMMP，详见同目录下的 NOTICE。

## 构建要求

- CMake 3.15+
- Flex 2.6.4+
- Bison 3.8.2+
- C++23 兼容编译器

## 构建步骤

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 使用方法

### 运行 REPL

```bash
./OpenLamina
```

### 运行文件

```bash
./OpenLamina script.lm
```