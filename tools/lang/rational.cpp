#include "rational.hpp"

#include <cmath>
#include <string>
#include <unordered_map>

namespace lang::lammp {
Rational Rational::fromDouble(long double x, long double epsilon) {
    if (std::isnan(x)) {
        throw RuntimeError("invalid numeric value: NaN");
    }
    if (std::isinf(x)) {
        throw RuntimeError("invalid numeric value: infinity");
    }

    const bool neg = x < 0;
    x = std::fabsl(x);

    if (x == 0.0L) {
        return Rational(Number(0));
    }

    long double h1 = 1;
    long double h2 = 0;
    long double k1 = 0;
    long double k2 = 1;
    long double b = x;
    constexpr long double max_k = 1e15L;

    for (int i = 0; i < 64; ++i) {
        const long double a = std::floor(b);
        const long double aux_h = h1;
        h1 = a * h1 + h2;
        h2 = aux_h;
        const long double aux_k = k1;
        k1 = a * k1 + k2;
        k2 = aux_k;

        if (k1 > max_k) {
            break;
        }
        if (std::fabsl(x - h1 / k1) <= epsilon) {
            break;
        }

        const long double frac = b - a;
        if (frac < epsilon) {
            break;
        }
        b = 1.0L / frac;
    }

    Number num = Number::fromString(std::to_string(static_cast<long long>(h1)));
    Number den = Number::fromString(std::to_string(static_cast<long long>(k1)));
    Rational result(num, den);
    return neg ? -result : result;
}

std::string Rational::toDecimalString(const size_t max_fraction_digits) const {
    if (denominator_ == Number(1)) {
        return numerator_.toString();
    }
    if (numerator_.isZero()) {
        return "0";
    }

    Number num = numerator_.abs();
    Number den = denominator_.abs();
    const bool negative = numerator_.isNegative();

    std::string result;
    if (negative) {
        result += '-';
    }

    Number int_part = num / den;
    Number rem = num % den;
    result += int_part.toString();

    if (rem.isZero()) {
        return result;
    }

    result += '.';

    std::unordered_map<std::string, size_t> seen;
    std::string frac;
    const Number ten(10);

    while (!rem.isZero()) {
        const std::string rem_key = rem.toString();
        if (const auto it = seen.find(rem_key); it != seen.end()) {
            const std::string non_repeat = frac.substr(0, it->second);
            const std::string repeat = frac.substr(it->second);
            result += non_repeat;
            result += '(';
            result += repeat;
            result += ')';
            return result;
        }

        if (frac.size() >= max_fraction_digits) {
            result += frac;
            result += "...";
            return result;
        }

        seen.emplace(rem_key, frac.size());

        rem = rem * ten;
        const Number digit = rem / den;
        rem = rem % den;
        frac += digit.toString();
    }

    result += frac;
    return result;
}
} // namespace lang::lammp