#pragma once

#include <_deps/lammp-src/include/lammp/lmmp.h>
#include <_deps/lammp-src/include/lammp/lmmpn.h>
#include <string>
#include <memory>
#include <variant>
#include <stdexcept>
#include <cstring>
#include <vector>

namespace lang::lammp {

class Number {
    struct BigNum {
        std::unique_ptr<mp_limb_t[]> data;
        mp_size_t size = 0;
        bool is_negative = false;
        mp_size_t decimal_digits = 0;

        BigNum() = default;
        BigNum(int64_t value);
        BigNum(uint64_t value);
        BigNum(const std::string& str, int base);
        BigNum(const BigNum& other);
        BigNum(BigNum&& other) noexcept;
        ~BigNum() = default;

        BigNum& operator=(const BigNum& other);
        BigNum& operator=(BigNum&& other) noexcept;

        void init_zero();
        void allocate(mp_size_t limbs);
        bool isZero() const;

        [[nodiscard]] std::string toString(const int base = 10) const {
            const BigNum& bn = *this;
            if (bn.isZero()) {
                return "0";
            }

            if (bn.data == nullptr || bn.size == 0) {
                return "0";
            }

            mp_size_t max_len = lmmp_to_str_len_(bn.data.get(), bn.size, base);

            std::vector<mp_byte_t> buffer(max_len + 10, 0);

            mp_size_t actual_len = lmmp_to_str_(buffer.data(), bn.data.get(), bn.size, base);

            std::string result;
            result.reserve(actual_len);

            for (mp_size_t i = 0; i < actual_len; ++i) {
                const mp_byte_t digit = buffer[actual_len - 1 - i];
                if (digit < 10) {
                    result += static_cast<char>('0' + digit);
                } else {
                    result += static_cast<char>('a' + (digit - 10));
                }
            }

            if (bn.is_negative) {
                return "-" + result;
            }
            return result;
        };
    };

    std::variant<int64_t, BigNum> value;

    explicit Number(const BigNum& bn) : value(bn) {}
    explicit Number(BigNum&& bn) noexcept : value(std::move(bn)) {}

    [[nodiscard]] bool is_small() const { return std::holds_alternative<int64_t>(value); }
    [[nodiscard]] int64_t get_small() const { return std::get<int64_t>(value); }
    [[nodiscard]] const BigNum& get_big() const { return std::get<BigNum>(value); }
    BigNum& get_big() { return std::get<BigNum>(value); }
    
    void upgrade_to_big();
    void upgrade_to_big(int64_t val);
    static BigNum add_big(const BigNum& a, const BigNum& b);
    static BigNum sub_big(const BigNum& a, const BigNum& b);
    static BigNum mul_big(const BigNum& a, const BigNum& b);
    static BigNum div_big(const BigNum& a, const BigNum& b);
    static BigNum mod_big(const BigNum& a, const BigNum& b);
    static bool cmp_big(const BigNum& a, const BigNum& b);
    static int compare_big(const BigNum& a, const BigNum& b);

public:
    Number();

    explicit Number(int64_t value);
    explicit Number(uint64_t value);
    explicit Number(const std::string& str, int base = 10);
    Number(const Number& other);
    Number(Number&& other) noexcept;
    ~Number();

    Number& operator=(const Number& other);
    Number& operator=(Number&& other) noexcept;

    [[nodiscard]] bool isZero() const;
    [[nodiscard]] bool isNegative() const;
    [[nodiscard]] bool isSmall() const { return is_small(); }

    Number operator-() const;
    Number operator+(const Number& other) const;
    Number operator-(const Number& other) const;
    Number operator*(const Number& other) const;
    Number operator/(const Number& other) const;
    Number operator%(const Number& other) const;

    Number& operator+=(const Number& other);
    Number& operator-=(const Number& other);
    Number& operator*=(const Number& other);
    Number& operator/=(const Number& other);
    Number& operator%=(const Number& other);

    bool operator==(const Number& other) const;
    bool operator!=(const Number& other) const;
    bool operator<(const Number& other) const;
    bool operator<=(const Number& other) const;
    bool operator>(const Number& other) const;
    bool operator>=(const Number& other) const;

    [[nodiscard]] std::string toString(int base = 10) const;
    [[nodiscard]] int64_t toInt64() const;
    [[nodiscard]] uint64_t toUInt64() const;

    [[nodiscard]] Number pow(const Number& exponent) const;
    [[nodiscard]] Number sqrt() const;
    [[nodiscard]] Number gcd(const Number& other) const;
    [[nodiscard]] Number abs() const;

    static Number fromString(const std::string& str, int base = 10);
};

}