#pragma once

#include "number.hpp"
#include "../error.hpp"
#include <cmath>
#include <string>

namespace lang::lammp {
/**
 * @class Rational
 * @brief 有理数类，支持精确分数运算
 */
class Rational {
private:
    Number numerator_;   ///< 分子
    Number denominator_; ///< 分母

    /**
     * @brief 约分，将分数化为最简形式
     */
    void simplify() {
        if (denominator_.isZero()) {
            if (numerator_.isZero()) {
                throw RuntimeError("Undefined: 0/0");
            }
            throw RuntimeError("Division by zero");
        }

        if (numerator_.isZero()) {
            denominator_ = Number(1);
            return;
        }

        Number gcd = numerator_.gcd(denominator_);
        numerator_ = numerator_ / gcd;
        denominator_ = denominator_ / gcd;

        if (denominator_.isNegative()) {
            numerator_ = -numerator_;
            denominator_ = -denominator_;
        }
    }

public:
    /**
     * @brief 默认构造函数（0/1）
     */
    Rational() : numerator_(0), denominator_(1) {
    }

    /**
     * @brief 从整数构造
     * @param num 整数值
     */
    Rational(const Number& num) : numerator_(num), denominator_(1) {
    }

    /**
     * @brief 从分子和分母构造
     * @param numerator 分子
     * @param denominator 分母
     */
    Rational(const Number& numerator, const Number& denominator)
        : numerator_(numerator), denominator_(denominator) {
        simplify();
    }

    /**
     * @brief 拷贝构造函数
     * @param other 另一个Rational
     */
    Rational(const Rational& other) = default;

    /**
     * @brief 移动构造函数
     * @param other 另一个Rational
     */
    Rational(Rational&& other) noexcept = default;

    /**
     * @brief 拷贝赋值运算符
     * @param other 另一个Rational
     * @return 引用
     */
    Rational& operator=(const Rational& other) = default;

    /**
     * @brief 移动赋值运算符
     * @param other 另一个Rational
     * @return 引用
     */
    Rational& operator=(Rational&& other) noexcept = default;

    /**
     * @brief 获取分子
     * @return 分子
     */
    const Number& numerator() const { return numerator_; }

    /**
     * @brief 获取分母
     * @return 分母
     */
    const Number& denominator() const { return denominator_; }

    /**
     * @brief 转换为Number（执行除法）
     * @return Number值
     */
    Number toNumber() const {
        return numerator_ / denominator_;
    }

    /** @brief 转换为浮点数（用于超越函数等） */
    [[nodiscard]] long double toLongDouble() const {
        return std::stold(numerator_.toString()) / std::stold(denominator_.toString());
    }

    /** @brief 从浮点近似构造最简分数 */
    [[nodiscard]] static Rational fromDouble(long double x, long double epsilon = 1e-12L);

    /**
     * @brief 转换为字符串（分数形式）
     * @return 字符串表示
     */
    std::string toString() const {
        if (denominator_ == Number(1)) {
            return numerator_.toString();
        }
        return numerator_.toString() + "/" + denominator_.toString();
    }

    /**
     * @brief 转换为字符串（小数形式）
     * @param max_fraction_digits 非循环小数时的最大小数位数
     * @return 字符串表示；循环节用括号标注，如 1/3 -> "0.(3)"
     */
    [[nodiscard]] std::string toDecimalString(size_t max_fraction_digits = 20) const;

    /**
     * @brief 一元负号
     * @return 负有理数
     */
    Rational operator-() const {
        return Rational(-numerator_, denominator_);
    }

    /**
     * @brief 加法
     * @param other 另一个有理数
     * @return 和
     */
    Rational operator+(const Rational& other) const {
        return Rational(
            numerator_ * other.denominator_ + other.numerator_ * denominator_,
            denominator_ * other.denominator_
        );
    }

    /**
     * @brief 减法
     * @param other 另一个有理数
     * @return 差
     */
    Rational operator-(const Rational& other) const {
        return Rational(
            numerator_ * other.denominator_ - other.numerator_ * denominator_,
            denominator_ * other.denominator_
        );
    }

    /**
     * @brief 乘法
     * @param other 另一个有理数
     * @return 积
     */
    Rational operator*(const Rational& other) const {
        return Rational(
            numerator_ * other.numerator_,
            denominator_ * other.denominator_
        );
    }

    /**
     * @brief 除法
     * @param other 另一个有理数
     * @return 商
     */
    Rational operator/(const Rational& other) const {
        if (other.numerator_.isZero()) {
            throw RuntimeError("Division by zero");
        }
        return Rational(
            numerator_ * other.denominator_,
            denominator_ * other.numerator_
        );
    }

    /**
     * @brief 相等比较
     * @param other 另一个有理数
     * @return 如果相等返回true
     */
    bool operator==(const Rational& other) const {
        return numerator_ == other.numerator_ && denominator_ == other.denominator_;
    }

    /**
     * @brief 不等比较
     * @param other 另一个有理数
     * @return 如果不等返回true
     */
    bool operator!=(const Rational& other) const {
        return !(*this == other);
    }

    /**
     * @brief 小于比较
     * @param other 另一个有理数
     * @return 如果小于返回true
     */
    bool operator<(const Rational& other) const {
        return numerator_ * other.denominator_ < other.numerator_ * denominator_;
    }

    /**
     * @brief 小于等于比较
     * @param other 另一个有理数
     * @return 如果小于等于返回true
     */
    bool operator<=(const Rational& other) const {
        return *this < other || *this == other;
    }

    /**
     * @brief 大于比较
     * @param other 另一个有理数
     * @return 如果大于返回true
     */
    bool operator>(const Rational& other) const {
        return !(*this <= other);
    }

    /**
     * @brief 大于等于比较
     * @param other 另一个有理数
     * @return 如果大于等于返回true
     */
    bool operator>=(const Rational& other) const {
        return !(*this < other);
    }
};
}