#include "rational.hpp"

#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>

namespace lang::lammp {
namespace {

std::string trim_ascii_whitespace(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return str.substr(start, end - start);
}

} // namespace

bool Rational::looksLikeDecimalLiteral(const std::string& str) {
    const std::string trimmed = trim_ascii_whitespace(str);
    size_t idx = 0;
    if (idx < trimmed.size() && (trimmed[idx] == '+' || trimmed[idx] == '-')) {
        ++idx;
    }
    for (; idx < trimmed.size(); ++idx) {
        const char c = trimmed[idx];
        if (c == '.' || c == 'e' || c == 'E') {
            return true;
        }
    }
    return false;
}

Rational Rational::fromDecimalString(const std::string& str) {
    std::string s = trim_ascii_whitespace(str);
    if (s.empty()) {
        throw RuntimeError("empty numeric literal");
    }

    bool neg = false;
    if (s[0] == '+') {
        s = s.substr(1);
    } else if (s[0] == '-') {
        neg = true;
        s = s.substr(1);
    }
    if (s.empty()) {
        throw RuntimeError(std::string("invalid numeric literal: ") + str);
    }

    std::string exp_str;
    const size_t e_pos = s.find_first_of("eE");
    if (e_pos != std::string::npos) {
        exp_str = s.substr(e_pos + 1);
        s = s.substr(0, e_pos);
        if (s.empty()) {
            throw RuntimeError(std::string("invalid numeric literal: ") + str);
        }
    }

    std::string int_part;
    std::string frac_part;
    const size_t dot_pos = s.find('.');
    if (dot_pos == std::string::npos) {
        int_part = s;
    } else {
        int_part = s.substr(0, dot_pos);
        frac_part = s.substr(dot_pos + 1);
    }

    if (int_part.empty() && frac_part.empty()) {
        throw RuntimeError(std::string("invalid numeric literal: ") + str);
    }
    if (int_part.empty()) {
        int_part = "0";
    }

    Number numerator(int_part);
    Number denominator(1);

    if (!frac_part.empty()) {
        const Number frac_value(frac_part);
        const Number scale = Number(10).pow(Number(static_cast<int64_t>(frac_part.size())));
        numerator = numerator * scale + frac_value;
        denominator = denominator * scale;
    }

    if (!exp_str.empty()) {
        bool exp_neg = false;
        if (exp_str[0] == '+') {
            exp_str = exp_str.substr(1);
        } else if (exp_str[0] == '-') {
            exp_neg = true;
            exp_str = exp_str.substr(1);
        }
        if (exp_str.empty()) {
            throw RuntimeError(std::string("invalid numeric literal: ") + str);
        }
        const Number exp_value(exp_str);
        const Number ten(10);
        const Number power = ten.pow(exp_value);
        if (exp_neg) {
            denominator = denominator * power;
        } else {
            numerator = numerator * power;
        }
    }

    Rational result(numerator, denominator);
    return neg ? -result : result;
}

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