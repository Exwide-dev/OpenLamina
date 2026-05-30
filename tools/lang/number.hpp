#pragma once

#include <_deps/lammp-src/include/lammp/lmmp.h>
#include <_deps/lammp-src/include/lammp/lmmpn.h>
#include <string>
#include <memory>
#include <variant>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <concepts>

namespace lang::lammp {

/**
 * @brief 整数类型概念
 * @tparam T 类型参数
 */
template<typename T>
concept IntegerType = std::is_integral_v<T> and !std::same_as<T, bool>;

/**
 * @class Number
 * @brief 任意精度整数类，支持大整数运算
 */
class Number {
    /**
     * @struct BigNum
     * @brief 大整数内部表示
     */
    struct BigNum {
        std::unique_ptr<mp_limb_t[]> data;   ///< 大整数数据（limb数组）
        mp_size_t size = 0;                   ///< limb数量
        bool is_negative = false;             ///< 是否为负数
        mp_size_t decimal_digits = 0;         ///< 十进制位数
        
        /**
         * @brief 默认构造函数
         */
        BigNum() = default;
        
        /**
         * @brief 从整数构造
         * @tparam T 整数类型
         * @param value 整数值
         */
        template<std::integral T>
        explicit(false) BigNum(T value) {
            lmmp_global_init();
            if constexpr (std::is_signed_v<T>) {
                if (value == 0) {
                    init_zero();
                    return;
                }
                is_negative = value < 0;
                auto abs_val = static_cast<uint64_t>(is_negative ? -value : value);
                size = abs_val > LIMB_MAX ? 2 : 1;
                allocate(size);
                if (size == 1) {
                    data[0] = abs_val;
                } else {
                    data[0] = abs_val & LLIMB_MASK;
                    data[1] = abs_val >> 32;
                }
            } else {
                if (value == 0) {
                    init_zero();
                    return;
                }
                is_negative = false;
                auto abs_val = static_cast<uint64_t>(value);
                size = abs_val > LIMB_MAX ? 2 : 1;
                allocate(size);
                if (size == 1) {
                    data[0] = abs_val;
                } else {
                    data[0] = abs_val & LLIMB_MASK;
                    data[1] = abs_val >> 32;
                }
            }
        }
        
        /**
         * @brief 从字符串构造
         * @param str 字符串表示
         * @param base 进制
         */
        BigNum(const std::string& str, int base);
        
        /**
         * @brief 拷贝构造函数
         * @param other 另一个BigNum
         */
        BigNum(const BigNum& other);
        
        /**
         * @brief 移动构造函数
         * @param other 另一个BigNum
         */
        BigNum(BigNum&& other) noexcept;
        
        /**
         * @brief 析构函数
         */
        ~BigNum() = default;
        
        /**
         * @brief 拷贝赋值运算符
         * @param other 另一个BigNum
         * @return 引用
         */
        BigNum& operator=(const BigNum& other);
        
        /**
         * @brief 移动赋值运算符
         * @param other 另一个BigNum
         * @return 引用
         */
        BigNum& operator=(BigNum&& other) noexcept;
        
        /**
         * @brief 初始化为零
         */
        void init_zero();
        
        /**
         * @brief 分配limb空间
         * @param limbs limb数量
         */
        void allocate(mp_size_t limbs);
        
        /**
         * @brief 检查是否为零
         * @return 如果为零返回true
         */
        bool isZero() const;
        
        /**
         * @brief 转换为字符串
         * @param base 进制，默认为10
         * @return 字符串表示
         */
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

    std::variant<int64_t, BigNum> value; ///< 值存储（小整数或大整数）

    /**
     * @brief 从BigNum构造（私有）
     * @param bn BigNum
     */
    explicit Number(const BigNum& bn) : value(bn) {}
    
    /**
     * @brief 从BigNum移动构造（私有）
     * @param bn BigNum
     */
    explicit Number(BigNum&& bn) noexcept : value(std::move(bn)) {}
    
    /**
     * @brief 检查是否为小整数
     * @return 如果是小整数返回true
     */
    [[nodiscard]] bool is_small() const { return std::holds_alternative<int64_t>(value); }
    
    /**
     * @brief 获取小整数值
     * @return int64_t值
     */
    [[nodiscard]] int64_t get_small() const { return std::get<int64_t>(value); }
    
    /**
     * @brief 获取大整数引用（const）
     * @return BigNum引用
     */
    [[nodiscard]] const BigNum& get_big() const { return std::get<BigNum>(value); }
    
    /**
     * @brief 获取大整数引用
     * @return BigNum引用
     */
    BigNum& get_big() { return std::get<BigNum>(value); }
    
    /**
     * @brief 升级为大整数
     */
    void upgrade_to_big();
    
    /**
     * @brief 从整数值升级为大整数
     * @param val 整数值
     */
    void upgrade_to_big(int64_t val);
    
    /**
     * @brief 大整数加法
     * @param a 操作数1
     * @param b 操作数2
     * @return 和
     */
    static BigNum add_big(const BigNum& a, const BigNum& b);
    
    /**
     * @brief 大整数减法
     * @param a 被减数
     * @param b 减数
     * @return 差
     */
    static BigNum sub_big(const BigNum& a, const BigNum& b);
    
    /**
     * @brief 大整数乘法
     * @param a 操作数1
     * @param b 操作数2
     * @return 积
     */
    static BigNum mul_big(const BigNum& a, const BigNum& b);
    
    /**
     * @brief 大整数除法
     * @param a 被除数
     * @param b 除数
     * @return 商
     */
    static BigNum div_big(const BigNum& a, const BigNum& b);
    
    /**
     * @brief 大整数取模
     * @param a 被除数
     * @param b 除数
     * @return 余数
     */
    static BigNum mod_big(const BigNum& a, const BigNum& b);
    
    /**
     * @brief 大整数比较（小于）
     * @param a 操作数1
     * @param b 操作数2
     * @return 如果a < b返回true
     */
    static bool cmp_big(const BigNum& a, const BigNum& b);
    
    /**
     * @brief 大整数比较
     * @param a 操作数1
     * @param b 操作数2
     * @return 负数（a < b）、零（a == b）或正数（a > b）
     */
    static int compare_big(const BigNum& a, const BigNum& b);

public:
    /**
     * @brief 默认构造函数（零）
     */
    Number();

    /**
     * @brief 从整数构造
     * @tparam T 整数类型
     * @param value 整数值
     */
    template<std::integral T>
    explicit Number(T value) {
        lmmp_global_init();
        if constexpr (std::is_signed_v<T>) {
            this->value = static_cast<int64_t>(value);
        } else {
            uint64_t v = static_cast<uint64_t>(value);
            if (v <= INT64_MAX) {
                this->value = static_cast<int64_t>(v);
            } else {
                this->value = BigNum(v);
            }
        }
    }
    
    /**
     * @brief 从字符串构造
     * @param str 字符串表示
     * @param base 进制，默认为10
     */
    explicit Number(const std::string& str, int base = 10);
    
    /**
     * @brief 拷贝构造函数
     * @param other 另一个Number
     */
    Number(const Number& other);
    
    /**
     * @brief 移动构造函数
     * @param other 另一个Number
     */
    Number(Number&& other) noexcept;
    
    /**
     * @brief 析构函数
     */
    ~Number();

    /**
     * @brief 拷贝赋值运算符
     * @param other 另一个Number
     * @return 引用
     */
    Number& operator=(const Number& other);
    
    /**
     * @brief 移动赋值运算符
     * @param other 另一个Number
     * @return 引用
     */
    Number& operator=(Number&& other) noexcept;

    /**
     * @brief 检查是否为零
     * @return 如果为零返回true
     */
    [[nodiscard]] bool isZero() const;
    
    /**
     * @brief 检查是否为负数
     * @return 如果为负数返回true
     */
    [[nodiscard]] bool isNegative() const;
    
    /**
     * @brief 检查是否为小整数
     * @return 如果是小整数返回true
     */
    [[nodiscard]] bool isSmall() const { return is_small(); }

    /**
     * @brief 一元负号
     * @return 负数
     */
    Number operator-() const;
    
    /**
     * @brief 加法
     * @param other 另一个操作数
     * @return 和
     */
    Number operator+(const Number& other) const;
    
    /**
     * @brief 减法
     * @param other 另一个操作数
     * @return 差
     */
    Number operator-(const Number& other) const;
    
    /**
     * @brief 乘法
     * @param other 另一个操作数
     * @return 积
     */
    Number operator*(const Number& other) const;
    
    /**
     * @brief 除法
     * @param other 另一个操作数
     * @return 商
     */
    Number operator/(const Number& other) const;
    
    /**
     * @brief 取模
     * @param other 另一个操作数
     * @return 余数
     */
    Number operator%(const Number& other) const;

    /**
     * @brief 加法赋值
     * @param other 另一个操作数
     * @return 引用
     */
    Number& operator+=(const Number& other);
    
    /**
     * @brief 减法赋值
     * @param other 另一个操作数
     * @return 引用
     */
    Number& operator-=(const Number& other);
    
    /**
     * @brief 乘法赋值
     * @param other 另一个操作数
     * @return 引用
     */
    Number& operator*=(const Number& other);
    
    /**
     * @brief 除法赋值
     * @param other 另一个操作数
     * @return 引用
     */
    Number& operator/=(const Number& other);
    
    /**
     * @brief 取模赋值
     * @param other 另一个操作数
     * @return 引用
     */
    Number& operator%=(const Number& other);

    /**
     * @brief 相等比较
     * @param other 另一个操作数
     * @return 如果相等返回true
     */
    bool operator==(const Number& other) const;
    
    /**
     * @brief 不等比较
     * @param other 另一个操作数
     * @return 如果不等返回true
     */
    bool operator!=(const Number& other) const;
    
    /**
     * @brief 小于比较
     * @param other 另一个操作数
     * @return 如果小于返回true
     */
    bool operator<(const Number& other) const;
    
    /**
     * @brief 小于等于比较
     * @param other 另一个操作数
     * @return 如果小于等于返回true
     */
    bool operator<=(const Number& other) const;
    
    /**
     * @brief 大于比较
     * @param other 另一个操作数
     * @return 如果大于返回true
     */
    bool operator>(const Number& other) const;
    
    /**
     * @brief 大于等于比较
     * @param other 另一个操作数
     * @return 如果大于等于返回true
     */
    bool operator>=(const Number& other) const;

    /**
     * @brief 转换为字符串
     * @param base 进制，默认为10
     * @return 字符串表示
     */
    [[nodiscard]] std::string toString(int base = 10) const;
    
    /**
     * @brief 转换为int64_t
     * @return int64_t值
     */
    [[nodiscard]] int64_t toInt64() const;
    
    /**
     * @brief 转换为uint64_t
     * @return uint64_t值
     */
    [[nodiscard]] uint64_t toUInt64() const;

    /**
     * @brief 幂运算
     * @param exponent 指数
     * @return 结果
     */
    [[nodiscard]] Number pow(const Number& exponent) const;
    
    /**
     * @brief 平方根
     * @return 平方根
     */
    [[nodiscard]] Number sqrt() const;
    
    /**
     * @brief 最大公约数
     * @param other 另一个操作数
     * @return GCD
     */
    [[nodiscard]] Number gcd(const Number& other) const;
    
    /**
     * @brief 绝对值
     * @return 绝对值
     */
    [[nodiscard]] Number abs() const;

    /**
     * @brief 从字符串解析
     * @param str 字符串表示
     * @param base 进制，默认为10
     * @return Number对象
     */
    static Number fromString(const std::string& str, int base = 10);
};

}