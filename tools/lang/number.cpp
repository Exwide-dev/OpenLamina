#include "number.hpp"
#include <iostream>
#include "../tools/debug.hpp"
#include "_deps/lammp-src/include/lammp/lmmpn.h"

#include <cmath>

namespace lang::lammp {
void Number::BigNum::init_zero() {
    size = 1;
    data = std::unique_ptr<mp_limb_t[]>(new mp_limb_t[size]);
    lmmp_zero(data.get(), size);
    is_negative = false;
    decimal_digits = 1;
}

void Number::BigNum::allocate(mp_size_t limbs) {
    if (limbs == 0) limbs = 1;
    data = std::unique_ptr<mp_limb_t[]>(new mp_limb_t[limbs]);
    lmmp_zero(data.get(), limbs);
    size = limbs;
}

bool Number::BigNum::isZero() const {
    if (!data || size == 0) return true;
    for (mp_size_t i = 0; i < size; ++i) {
        if (data[i] != 0) return false;
    }
    return true;
}


Number::BigNum::BigNum(const std::string& str, int base) {
    lmmp_global_init();
    if (str.empty()) {
        init_zero();
        return;
    }
    size_t start = 0;
    if (str[0] == '-') {
        is_negative = true;
        start = 1;
    } else if (str[0] == '+') {
        is_negative = false;
        start = 1;
    }
    std::string num_str = str.substr(start);
    decimal_digits = num_str.size();
    if (num_str.empty()) {
        init_zero();
        return;
    }

    std::vector<mp_byte_t> digits(num_str.size());
    // lammp_from_str_ 期望最低有效位在前，所以需要反转字符串
    for (size_t i = 0; i < num_str.size(); ++i) {
        char c = num_str[num_str.size() - 1 - i];
        if (c >= '0' && c <= '9') {
            digits[i] = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digits[i] = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digits[i] = 10 + (c - 'A');
        } else {
            digits[i] = 0;
        }
    }

    mp_size_t needed = lmmp_from_str_len_(nullptr, digits.size(), base);
    allocate(needed);
    size = lmmp_from_str_(data.get(), digits.data(), digits.size(), base);
}

Number::BigNum::BigNum(const BigNum& other) {
    lmmp_global_init();
    if (other.size > 0 && other.data) {
        allocate(other.size);
        lmmp_copy(data.get(), other.data.get(), other.size);
        is_negative = other.is_negative;
        decimal_digits = other.decimal_digits;
    } else {
        init_zero();
    }
}

Number::BigNum::BigNum(BigNum&& other) noexcept {
    lmmp_global_init();
    data = std::move(other.data);
    size = other.size;
    is_negative = other.is_negative;
    decimal_digits = other.decimal_digits;
    other.size = 0;
    other.is_negative = false;
    other.decimal_digits = 0;
}

Number::BigNum& Number::BigNum::operator=(const BigNum& other) {
    if (this != &other) {
        if (other.size > 0 && other.data) {
            allocate(other.size);
            lmmp_copy(data.get(), other.data.get(), other.size);
            is_negative = other.is_negative;
            decimal_digits = other.decimal_digits;
        } else {
            init_zero();
        }
    }
    return *this;
}

Number::BigNum& Number::BigNum::operator=(BigNum&& other) noexcept {
    if (this != &other) {
        data = std::move(other.data);
        size = other.size;
        is_negative = other.is_negative;
        decimal_digits = other.decimal_digits;
        other.size = 0;
        other.is_negative = false;
        other.decimal_digits = 0;
    }
    return *this;
}

void Number::upgrade_to_big() {
    if (is_small()) {
        value = BigNum(get_small());
    }
}

void Number::upgrade_to_big(int64_t val) {
    value = BigNum(val);
}

Number::BigNum Number::add_big(const BigNum& a, const BigNum& b) {
    BigNum result;
    if (a.is_negative == b.is_negative) {
        mp_size_t max_size = LMMP_MAX(a.size, b.size);
        result.allocate(max_size + 1);
        mp_limb_t carry = lmmp_add_n_(result.data.get(), a.data.get(), b.data.get(), LMMP_MIN(a.size, b.size));
        if (a.size > b.size) {
            lmmp_copy(result.data.get() + b.size, a.data.get() + b.size, a.size - b.size);
            if (carry != 0) {
                mp_size_t i = b.size;
                while (i < a.size && carry != 0) {
                    carry = lmmp_add_1_(result.data.get() + i, a.data.get() + i, 1, carry);
                    i++;
                }
                if (carry != 0) {
                    result.data[max_size] = 1;
                    result.size = max_size + 1;
                    result.is_negative = a.is_negative;
                    return result;
                }
            }
        } else if (b.size > a.size) {
            lmmp_copy(result.data.get() + a.size, b.data.get() + a.size, b.size - a.size);
            if (carry != 0) {
                mp_size_t i = a.size;
                while (i < b.size && carry != 0) {
                    carry = lmmp_add_1_(result.data.get() + i, b.data.get() + i, 1, carry);
                    i++;
                }
                if (carry != 0) {
                    result.data[max_size] = 1;
                    result.size = max_size + 1;
                    result.is_negative = a.is_negative;
                    return result;
                }
            }
        } else if (carry != 0) {
            result.data[max_size] = 1;
            result.size = max_size + 1;
            result.is_negative = a.is_negative;
            return result;
        }
        result.size = max_size;
        result.is_negative = a.is_negative;
        return result;
    }

    BigNum abs_a = a, abs_b = b;
    abs_a.is_negative = false;
    abs_b.is_negative = false;

    int cmp = compare_big(abs_a, abs_b);
    if (cmp < 0) {
        BigNum r = sub_big(abs_b, abs_a);
        r.is_negative = b.is_negative;
        return r;
    } else {
        BigNum r = sub_big(abs_a, abs_b);
        r.is_negative = a.is_negative;
        return r;
    }
}

Number::BigNum Number::sub_big(const BigNum& a, const BigNum& b) {
    BigNum result;
    result.allocate(a.size);

    if (a.size == b.size) {
        lmmp_sub_n_(result.data.get(), a.data.get(), b.data.get(), a.size);
    } else {
        lmmp_sub_n_(result.data.get(), a.data.get(), b.data.get(), b.size);
        lmmp_copy(result.data.get() + b.size, a.data.get() + b.size, a.size - b.size);
    }

    result.size = a.size;
    while (result.size > 1 && result.data[result.size - 1] == 0) {
        result.size--;
    }
    result.is_negative = false;
    return result;
}

Number::BigNum Number::mul_big(const BigNum& a, const BigNum& b) {
    if (a.isZero() || b.isZero()) {
        BigNum r;
        r.init_zero();
        return r;
    }

    BigNum result;
    mp_size_t result_size = a.size + b.size;
    result.allocate(result_size);

    if (a.size >= b.size) {
        lmmp_mul_(result.data.get(), a.data.get(), a.size, b.data.get(), b.size);
    } else {
        lmmp_mul_(result.data.get(), b.data.get(), b.size, a.data.get(), a.size);
    }

    result.size = result_size;
    while (result.size > 1 && result.data[result.size - 1] == 0) {
        result.size--;
    }
    result.is_negative = a.is_negative != b.is_negative;
    return result;
}

Number::BigNum Number::div_big(const BigNum& a, const BigNum& b) {
    if (b.isZero()) throw std::runtime_error("Division by zero");
    if (a.isZero()) {
        BigNum r;
        r.init_zero();
        return r;
    }

    BigNum dividend = a, divisor = b;
    dividend.is_negative = false;
    divisor.is_negative = false;

    int cmp = compare_big(dividend, divisor);
    if (cmp < 0) {
        BigNum r;
        r.init_zero();
        return r;
    }

    BigNum result;
    result.allocate(dividend.size - divisor.size + 1);

    std::unique_ptr<mp_limb_t[]> remainder(new mp_limb_t[divisor.size]);
    lmmp_div_(result.data.get(), remainder.get(), dividend.data.get(), dividend.size, divisor.data.get(), divisor.size);

    result.size = dividend.size - divisor.size + 1;
    while (result.size > 1 && result.data[result.size - 1] == 0) {
        result.size--;
    }

    result.is_negative = a.is_negative != b.is_negative;
    return result;
}

Number::BigNum Number::mod_big(const BigNum& a, const BigNum& b) {
    if (b.isZero()) throw std::runtime_error("Modulo by zero");
    if (a.isZero()) {
        BigNum r;
        r.init_zero();
        return r;
    }

    BigNum dividend = a, divisor = b;
    dividend.is_negative = false;
    divisor.is_negative = false;

    int cmp = compare_big(dividend, divisor);
    if (cmp < 0) {
        BigNum r = a;
        return r;
    }

    BigNum result;
    result.allocate(divisor.size);

    std::unique_ptr<mp_limb_t[]> quotient(new mp_limb_t[dividend.size - divisor.size + 1]);
    lmmp_div_(quotient.get(), result.data.get(), dividend.data.get(), dividend.size, divisor.data.get(), divisor.size);

    result.size = divisor.size;
    while (result.size > 1 && result.data[result.size - 1] == 0) {
        result.size--;
    }
    result.is_negative = a.is_negative;
    return result;
}

bool Number::cmp_big(const BigNum& a, const BigNum& b) {
    if (a.is_negative != b.is_negative) {
        return a.is_negative;
    }
    if (a.size != b.size) {
        bool r = a.size < b.size;
        return a.is_negative ? !r : r;
    }
    int cmp = lmmp_cmp_(a.data.get(), b.data.get(), a.size);
    if (a.is_negative) {
        return cmp > 0;
    }
    return cmp < 0;
}

int Number::compare_big(const BigNum& a, const BigNum& b) {
    if (a.is_negative != b.is_negative) {
        return a.is_negative ? -1 : 1;
    }
    if (a.size != b.size) {
        return a.is_negative ? (a.size > b.size ? -1 : 1) : a.size > b.size ? 1 : -1;
    }
    int cmp = lmmp_cmp_(a.data.get(), b.data.get(), a.size);
    if (a.is_negative) {
        return -cmp;
    }
    return cmp;
}

Number::Number() : value(0LL) { lmmp_global_init(); }

Number::Number(const std::string& str, int base) {
    lmmp_global_init();
    try {
        int64_t v = std::stoll(str, nullptr, base);
        value = v;
    } catch (...) {
        value = BigNum(str, base);
    }
}

Number::Number(const Number& other) : value(other.value) {
}

Number::Number(Number&& other) noexcept : value(std::move(other.value)) {
}

Number::~Number() = default;

Number& Number::operator=(const Number& other) {
    if (this != &other) {
        value = other.value;
    }
    return *this;
}

Number& Number::operator=(Number&& other) noexcept {
    if (this != &other) {
        value = std::move(other.value);
    }
    return *this;
}

bool Number::isZero() const {
    if (is_small()) {
        return get_small() == 0;
    }
    return get_big().isZero();
}

bool Number::isNegative() const {
    if (is_small()) {
        return get_small() < 0;
    }
    return get_big().is_negative;
}

Number Number::operator-() const {
    if (is_small()) {
        return Number(-get_small());
    }
    Number result(*this);
    result.get_big().is_negative = !result.get_big().is_negative;
    return result;
}

Number Number::operator+(const Number& other) const {
    if (is_small() && other.is_small()) {
        LOG(ITIS(is_small() && other.is_small()));
        int64_t x = get_small(), y = other.get_small();
        if ((y > 0 && x > INT64_MAX - y) || (y < 0 && x < INT64_MIN - y)) {
            LOG(ITIS((y > 0 && x > INT64_MAX - y) || (y < 0 && x < INT64_MIN - y)) << "so add_big");
            return Number(add_big(BigNum(x), BigNum(y)));
        }
        LOG("small add");
        return Number(x + y);
    }

    BigNum a = is_small() ? BigNum(get_small()) : get_big();
    BigNum b = other.is_small() ? BigNum(other.get_small()) : other.get_big();
    LOG(ITIS(,a,.toString()));
    return Number(add_big(a, b));
}

Number Number::operator-(const Number& other) const {
    if (is_small() && other.is_small()) {
        int64_t x = get_small(), y = other.get_small();
        if ((y < 0 && x > INT64_MAX + y) || (y > 0 && x < INT64_MIN + y)) {
            return Number(sub_big(BigNum(x), BigNum(y)));
        }
        return Number(x - y);
    }

    BigNum a = is_small() ? BigNum(get_small()) : get_big();
    BigNum b = other.is_small() ? BigNum(other.get_small()) : other.get_big();
    return Number(sub_big(a, b));
}

Number Number::operator*(const Number& other) const {
    if (is_small() && other.is_small()) {
        int64_t x = get_small(), y = other.get_small();
        if (x == 0 || y == 0) return Number(0LL);

        uint64_t abs_x = static_cast<uint64_t>(x < 0 ? -x : x);
        uint64_t abs_y = static_cast<uint64_t>(y < 0 ? -y : y);

        if (abs_x > UINT64_MAX / abs_y) {
            return Number(mul_big(BigNum(x), BigNum(y)));
        }

        uint64_t prod = abs_x * abs_y;
        bool neg = x < 0 != y < 0;

        if (prod > INT64_MAX) {
            return Number(mul_big(BigNum(x), BigNum(y)));
        }

        return Number(neg ? -static_cast<int64_t>(prod) : static_cast<int64_t>(prod));
    }

    BigNum a = is_small() ? BigNum(get_small()) : get_big();
    BigNum b = other.is_small() ? BigNum(other.get_small()) : other.get_big();
    return Number(mul_big(a, b));
}

Number Number::operator/(const Number& other) const {
    if (other.isZero()) throw std::runtime_error("Division by zero");

    if (is_small() && other.is_small()) {
        return Number(get_small() / other.get_small());
    }

    BigNum a = is_small() ? BigNum(get_small()) : get_big();
    BigNum b = other.is_small() ? BigNum(other.get_small()) : other.get_big();
    return Number(div_big(a, b));
}

Number Number::operator%(const Number& other) const {
    LOG("Exec modulo");
    if (other.isZero()) throw std::runtime_error("Modulo by zero");

    if (is_small() && other.is_small()) {
        return Number(get_small() % other.get_small());
    }

    BigNum a = is_small() ? BigNum(get_small()) : get_big();
    BigNum b = other.is_small() ? BigNum(other.get_small()) : other.get_big();
    return Number(mod_big(a, b));
}

Number& Number::operator+=(const Number& other) {
    *this = *this + other;
    return *this;
}

Number& Number::operator-=(const Number& other) {
    *this = *this - other;
    return *this;
}

Number& Number::operator*=(const Number& other) {
    *this = *this * other;
    return *this;
}

Number& Number::operator/=(const Number& other) {
    *this = *this / other;
    return *this;
}

Number& Number::operator%=(const Number& other) {
    *this = *this % other;
    return *this;
}

bool Number::operator==(const Number& other) const {
    if (is_small() && other.is_small()) {
        return get_small() == other.get_small();
    }
    if (!is_small() && !other.is_small()) {
        const BigNum& a = get_big();
        const BigNum& b = other.get_big();
        if (a.is_negative != b.is_negative || a.size != b.size) return false;
        return lmmp_cmp_(a.data.get(), b.data.get(), a.size) == 0;
    }

    if (isZero() && other.isZero()) return true;
    return false;
}

bool Number::operator!=(const Number& other) const { return !(*this == other); }

bool Number::operator<(const Number& other) const {
    if (is_small() && other.is_small()) {
        return get_small() < other.get_small();
    }
    if (!is_small() && !other.is_small()) {
        return cmp_big(get_big(), other.get_big());
    }

    if (is_small()) {
        BigNum a(get_small());
        return cmp_big(a, other.get_big());
    } else {
        BigNum b(other.get_small());
        return cmp_big(get_big(), b);
    }
}

bool Number::operator<=(const Number& other) const { return *this < other || *this == other; }
bool Number::operator>(const Number& other) const { return !(*this <= other); }
bool Number::operator>=(const Number& other) const { return !(*this < other); }

std::string Number::toString(int base) const {
    if (is_small()) {
        return std::to_string(get_small());
    }

    const BigNum& bn = get_big();
    return bn.toString();
}

int64_t Number::toInt64() const {
    if (is_small()) {
        return get_small();
    }
    const BigNum& bn = get_big();
    if (bn.isZero()) return 0;

    int64_t result = 0;
    mp_size_t max_limbs = sizeof(int64_t) / sizeof(mp_limb_t);
    for (mp_size_t i = 0; i < LMMP_MIN(bn.size, max_limbs); ++i) {
        result |= static_cast<int64_t>(bn.data[i]) << (i * 64);
    }

    return bn.is_negative ? -result : result;
}

uint64_t Number::toUInt64() const {
    if (is_small()) {
        return static_cast<uint64_t>(get_small());
    }
    const BigNum& bn = get_big();
    if (bn.isZero()) return 0;

    uint64_t result = 0;
    mp_size_t max_limbs = sizeof(uint64_t) / sizeof(mp_limb_t);
    for (mp_size_t i = 0; i < LMMP_MIN(bn.size, max_limbs); ++i) {
        result |= bn.data[i] << (i * 64);
    }

    return result;
}

Number Number::pow(const Number& exponent) const {
    if (exponent.isZero()) return Number(1LL);
    if (isZero()) return Number(0LL);

    Number base = *this;
    Number exp = exponent;
    Number result(1LL);

    while (!exp.isZero()) {
        if (exp.is_small() ? exp.get_small() & 1 : exp.get_big().data[0] & 1) {
            result = result * base;
        }
        base = base * base;
        exp = exp / Number(2LL);
    }

    return result;
}

Number Number::sqrt() const {
    if (isNegative()) throw std::runtime_error("Cannot compute square root of negative number");
    if (isZero()) return Number(0LL);

    if (is_small()) {
        return Number(static_cast<int64_t>(std::sqrt(get_small())));
    }

    const BigNum& bn = get_big();
    BigNum result;
    mp_size_t result_size = (bn.size + 1) / 2 + 1;
    result.allocate(result_size);

    std::unique_ptr<mp_limb_t[]> remainder(new mp_limb_t[result_size]);
    lmmp_zero(remainder.get(), result_size);

    lmmp_sqrt_(result.data.get(), remainder.get(), bn.data.get(), bn.size, 0);

    result.size = result_size;
    while (result.size > 1 && result.data[result.size - 1] == 0) {
        result.size--;
    }
    result.is_negative = false;

    return Number(result);
}

Number Number::gcd(const Number& other) const {
    Number a = abs();
    Number b = other.abs();

    while (!b.isZero()) {
        Number temp = b;
        b = a % b;
        a = temp;
    }

    return a;
}

Number Number::abs() const {
    if (is_small()) {
        int64_t v = get_small();
        return Number(v < 0 ? -v : v);
    }
    Number result(*this);
    result.get_big().is_negative = false;
    return result;
}

Number Number::fromString(const std::string& str, int base) {
    return Number(str, base);
}
}
