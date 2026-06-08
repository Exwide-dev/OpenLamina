#include "rational.hpp"

#include <string>
#include <unordered_map>

namespace lang::lammp {
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