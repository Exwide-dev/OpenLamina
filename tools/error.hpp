#pragma once

#include "error.hpp"
#include <stdexcept>

class SyntaxError final : public std::runtime_error {
public:
    explicit SyntaxError(const std::string &msg) : std::runtime_error(msg) {}
};

class RuntimeError final : public std::runtime_error {
public:
    explicit RuntimeError(const std::string &msg) : std::runtime_error(msg) {}
};