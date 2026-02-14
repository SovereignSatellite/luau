// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "lualib.h"

#include "lcommon.h"
#include "lnumutils.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>

// portable count leading zeros
static int countlz64(uint64_t n)
{
    if (n == 0)
        return 64;

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    unsigned long r;
    _BitScanReverse64(&r, n);
    return 63 - int(r);
#elif defined(__GNUC__)
    return __builtin_clzll(n);
#else
    int r = 0;
    if (n >> 32 == 0)
    {
        r += 32;
        n <<= 32;
    }
    if (n >> 48 == 0)
    {
        r += 16;
        n <<= 16;
    }
    if (n >> 56 == 0)
    {
        r += 8;
        n <<= 8;
    }
    if (n >> 60 == 0)
    {
        r += 4;
        n <<= 4;
    }
    if (n >> 62 == 0)
    {
        r += 2;
        n <<= 2;
    }
    if (n >> 63 == 0)
    {
        r += 1;
    }
    return r;
#endif
}

// portable count trailing zeros
static int countrz64(uint64_t n)
{
    if (n == 0)
        return 64;

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    unsigned long r;
    _BitScanForward64(&r, n);
    return int(r);
#elif defined(__GNUC__)
    return __builtin_ctzll(n);
#else
    int r = 0;
    if ((n & 0xFFFFFFFF) == 0)
    {
        r += 32;
        n >>= 32;
    }
    if ((n & 0xFFFF) == 0)
    {
        r += 16;
        n >>= 16;
    }
    if ((n & 0xFF) == 0)
    {
        r += 8;
        n >>= 8;
    }
    if ((n & 0xF) == 0)
    {
        r += 4;
        n >>= 4;
    }
    if ((n & 0x3) == 0)
    {
        r += 2;
        n >>= 2;
    }
    if ((n & 0x1) == 0)
    {
        r += 1;
    }
    return r;
#endif
}

static uint64_t byteswap64(uint64_t n)
{
#if defined(_MSC_VER)
    return _byteswap_uint64(n);
#elif defined(__GNUC__)
    return __builtin_bswap64(n);
#else
    return ((n & 0x00000000000000FFull) << 56) | ((n & 0x000000000000FF00ull) << 40) | ((n & 0x0000000000FF0000ull) << 24) |
           ((n & 0x00000000FF000000ull) << 8) | ((n & 0x000000FF00000000ull) >> 8) | ((n & 0x0000FF0000000000ull) >> 24) |
           ((n & 0x00FF000000000000ull) >> 40) | ((n & 0xFF00000000000000ull) >> 56);
#endif
}

static const int64_t INT64_MIN_VALUE = int64_t(0x8000000000000000ull);

static int intlib_create(lua_State* L)
{
    double n = luaL_checknumber(L, 1);

    // must be exactly representable: no fractional part, in range, not NaN
    if (n != n) // NaN
    {
        lua_pushnil(L);
        return 1;
    }

    // check range: [-2^63, 2^63-1]
    if (n < (double)INT64_MIN_VALUE || n > (double)INT64_MAX)
    {
        lua_pushnil(L);
        return 1;
    }

    int64_t i = (int64_t)n;
    if ((double)i != n)
    {
        lua_pushnil(L);
        return 1;
    }

    lua_pushinteger64(L, i);
    return 1;
}

static int intlib_fromstring(lua_State* L)
{
    size_t len;
    const char* s = luaL_checklstring(L, 1, &len);
    int base = (int)luaL_optinteger(L, 2, 0);

    if (base != 0 && (base < 2 || base > 36))
    {
        luaL_argerror(L, 2, "invalid base");
    }

    // skip leading whitespace
    while (len > 0 && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v'))
    {
        s++;
        len--;
    }

    // skip trailing whitespace
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == '\f' ||
                        s[len - 1] == '\v'))
    {
        len--;
    }

    if (len == 0)
    {
        lua_pushnil(L);
        return 1;
    }

    // handle sign
    int negative = 0;
    if (*s == '-')
    {
        negative = 1;
        s++;
        len--;
    }
    else if (*s == '+')
    {
        s++;
        len--;
    }

    if (len == 0)
    {
        lua_pushnil(L);
        return 1;
    }

    int actualBase = base;

    // auto-detect or handle 0x prefix
    if (base == 0)
    {
        if (len >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        {
            actualBase = 16;
            s += 2;
            len -= 2;
        }
        else
        {
            actualBase = 10;
        }
    }
    else if ((base == 10 || base == 16) && len >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        // In base 10 and base 16, number is allowed to have a 0x or 0X prefix
        actualBase = 16;
        s += 2;
        len -= 2;
    }

    if (len == 0)
    {
        lua_pushnil(L);
        return 1;
    }

    // need a null-terminated copy for strtoull
    char buf[128];
    if (len >= sizeof(buf))
    {
        lua_pushnil(L);
        return 1;
    }
    memcpy(buf, s, len);
    buf[len] = '\0';

    char* end = NULL;
    errno = 0;
    unsigned long long value = strtoull(buf, &end, actualBase);

    if (end != buf + len)
    {
        lua_pushnil(L);
        return 1;
    }

    if (errno == ERANGE)
    {
        // overflow during parsing
        lua_pushnil(L);
        return 1;
    }

    int64_t result;
    if (negative)
    {
        if (value > (uint64_t)INT64_MIN_VALUE)
        {
            lua_pushnil(L);
            return 1;
        }
        result = -(int64_t)value;
    }
    else
    {
        result = (int64_t)value;
    }

    lua_pushinteger64(L, result);
    return 1;
}

static int intlib_tostring(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)n);
    lua_pushstring(L, buf);
    return 1;
}

static int intlib_tonumber(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    lua_pushnumber(L, (double)n);
    return 1;
}

static int intlib_neg(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    lua_pushinteger64(L, -a); // wraps on overflow per two's complement
    return 1;
}

static int intlib_add(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushinteger64(L, a + b); // wraps on overflow
    return 1;
}

static int intlib_sub(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushinteger64(L, a - b); // wraps on overflow
    return 1;
}

static int intlib_mul(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    // Use unsigned multiplication to avoid UB on signed overflow
    lua_pushinteger64(L, (int64_t)((uint64_t)a * (uint64_t)b));
    return 1;
}

static int intlib_div(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);

    if (b == 0)
        luaL_error(L, "division by zero");
    if (a == INT64_MIN_VALUE && b == -1)
        luaL_error(L, "overflow");

    lua_pushinteger64(L, a / b); // truncated division
    return 1;
}

static int intlib_rem(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);

    if (b == 0)
        luaL_error(L, "division by zero");
    if (a == INT64_MIN_VALUE && b == -1)
    {
        lua_pushinteger64(L, 0);
        return 1;
    }

    lua_pushinteger64(L, a % b);
    return 1;
}

static int intlib_idiv(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);

    if (b == 0)
        luaL_error(L, "division by zero");
    if (a == INT64_MIN_VALUE && b == -1)
        luaL_error(L, "overflow");

    // floored division: result is floor(a/b)
    int64_t q = a / b;
    int64_t r = a % b;
    // adjust if remainder has different sign than divisor
    if (r != 0 && ((r ^ b) < 0))
        q -= 1;

    lua_pushinteger64(L, q);
    return 1;
}

static int intlib_mod(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);

    if (b == 0)
        luaL_error(L, "division by zero");
    if (a == INT64_MIN_VALUE && b == -1)
    {
        lua_pushinteger64(L, 0);
        return 1;
    }

    // floored modulus: result has same sign as divisor
    int64_t r = a % b;
    if (r != 0 && ((r ^ b) < 0))
        r += b;

    lua_pushinteger64(L, r);
    return 1;
}

static int intlib_udiv(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);

    uint64_t ua = (uint64_t)a;
    uint64_t ub = (uint64_t)b;

    if (ub == 0)
        luaL_error(L, "division by zero");

    lua_pushinteger64(L, (int64_t)(ua / ub));
    return 1;
}

static int intlib_urem(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);

    uint64_t ua = (uint64_t)a;
    uint64_t ub = (uint64_t)b;

    if (ub == 0)
        luaL_error(L, "division by zero");

    lua_pushinteger64(L, (int64_t)(ua % ub));
    return 1;
}

static int intlib_min(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushinteger64(L, a < b ? a : b);
    return 1;
}

static int intlib_max(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushinteger64(L, a > b ? a : b);
    return 1;
}

static int intlib_clamp(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t lo = luaL_checkinteger64(L, 2);
    int64_t hi = luaL_checkinteger64(L, 3);

    if (lo > hi)
        luaL_error(L, "max must be greater than or equal to min");

    if (a < lo)
        lua_pushinteger64(L, lo);
    else if (a > hi)
        lua_pushinteger64(L, hi);
    else
        lua_pushinteger64(L, a);
    return 1;
}

static int intlib_band(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushinteger64(L, a & b);
    return 1;
}

static int intlib_bor(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushinteger64(L, a | b);
    return 1;
}

static int intlib_bnot(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    lua_pushinteger64(L, ~a);
    return 1;
}

static int intlib_bxor(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushinteger64(L, a ^ b);
    return 1;
}

static int intlib_lt(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushboolean(L, a < b);
    return 1;
}

static int intlib_le(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushboolean(L, a <= b);
    return 1;
}

static int intlib_ult(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushboolean(L, (uint64_t)a < (uint64_t)b);
    return 1;
}

static int intlib_ule(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushboolean(L, (uint64_t)a <= (uint64_t)b);
    return 1;
}

static int intlib_lshift(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    int64_t i = luaL_checkinteger64(L, 2);

    if (i < -63 || i > 63)
    {
        lua_pushinteger64(L, 0);
        return 1;
    }

    uint64_t un = (uint64_t)n;
    if (i >= 0)
        lua_pushinteger64(L, (int64_t)(un << i));
    else
        lua_pushinteger64(L, (int64_t)(un >> (-i)));
    return 1;
}

static int intlib_rshift(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    int64_t i = luaL_checkinteger64(L, 2);

    if (i < -63 || i > 63)
    {
        lua_pushinteger64(L, 0);
        return 1;
    }

    uint64_t un = (uint64_t)n;
    if (i >= 0)
        lua_pushinteger64(L, (int64_t)(un >> i));
    else
        lua_pushinteger64(L, (int64_t)(un << (-i)));
    return 1;
}

static int intlib_arshift(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    int64_t i = luaL_checkinteger64(L, 2);

    if (i > 63)
    {
        // returns integer with all bits set to sign bit
        lua_pushinteger64(L, n >> 63);
        return 1;
    }

    if (i < -63)
    {
        lua_pushinteger64(L, 0);
        return 1;
    }

    if (i >= 0)
        lua_pushinteger64(L, n >> i); // arithmetic shift (implementation-defined but guaranteed on most platforms)
    else
        lua_pushinteger64(L, (int64_t)((uint64_t)n << (-i)));
    return 1;
}

static int intlib_lrotate(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    int64_t i = luaL_checkinteger64(L, 2);

    // i is interpreted modulo 64
    int shift = (int)(i % 64);
    if (shift < 0)
        shift += 64;

    uint64_t un = (uint64_t)n;
    if (shift == 0)
        lua_pushinteger64(L, n);
    else
        lua_pushinteger64(L, (int64_t)((un << shift) | (un >> (64 - shift))));
    return 1;
}

static int intlib_rrotate(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    int64_t i = luaL_checkinteger64(L, 2);

    // i is interpreted modulo 64
    int shift = (int)(i % 64);
    if (shift < 0)
        shift += 64;

    uint64_t un = (uint64_t)n;
    if (shift == 0)
        lua_pushinteger64(L, n);
    else
        lua_pushinteger64(L, (int64_t)((un >> shift) | (un << (64 - shift))));
    return 1;
}

static int intlib_extract(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    int64_t f = luaL_checkinteger64(L, 2);
    int64_t w = luaL_optinteger64(L, 3, 1);

    if (f < 0)
        luaL_error(L, "field cannot be negative");
    if (w <= 0)
        luaL_error(L, "width must be positive");
    if (f + w > 64)
        luaL_error(L, "trying to access bits outside the range");

    uint64_t mask = (w == 64) ? ~(uint64_t)0 : ((uint64_t)1 << w) - 1;
    lua_pushinteger64(L, (int64_t)(((uint64_t)n >> f) & mask));
    return 1;
}

static int intlib_replace(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    int64_t r = luaL_checkinteger64(L, 2);
    int64_t f = luaL_checkinteger64(L, 3);
    int64_t w = luaL_optinteger64(L, 4, 1);

    if (f < 0)
        luaL_error(L, "field cannot be negative");
    if (w <= 0)
        luaL_error(L, "width must be positive");
    if (f + w > 64)
        luaL_error(L, "trying to access bits outside the range");

    uint64_t mask = (w == 64) ? ~(uint64_t)0 : ((uint64_t)1 << w) - 1;
    uint64_t un = (uint64_t)n;

    un = (un & ~(mask << f)) | (((uint64_t)r & mask) << f);
    lua_pushinteger64(L, (int64_t)un);
    return 1;
}

static int intlib_btest(lua_State* L)
{
    int64_t a = luaL_checkinteger64(L, 1);
    int64_t b = luaL_checkinteger64(L, 2);
    lua_pushboolean(L, (a & b) != 0);
    return 1;
}

static int intlib_countrz(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    lua_pushinteger64(L, countrz64((uint64_t)n));
    return 1;
}

static int intlib_countlz(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    lua_pushinteger64(L, countlz64((uint64_t)n));
    return 1;
}

static int intlib_bswap(lua_State* L)
{
    int64_t n = luaL_checkinteger64(L, 1);
    lua_pushinteger64(L, (int64_t)byteswap64((uint64_t)n));
    return 1;
}

static const luaL_Reg integerlib[] = {
    {"create", intlib_create},
    {"fromstring", intlib_fromstring},
    {"tostring", intlib_tostring},
    {"tonumber", intlib_tonumber},
    {"neg", intlib_neg},
    {"add", intlib_add},
    {"sub", intlib_sub},
    {"mul", intlib_mul},
    {"div", intlib_div},
    {"rem", intlib_rem},
    {"idiv", intlib_idiv},
    {"mod", intlib_mod},
    {"udiv", intlib_udiv},
    {"urem", intlib_urem},
    {"min", intlib_min},
    {"max", intlib_max},
    {"clamp", intlib_clamp},
    {"band", intlib_band},
    {"bor", intlib_bor},
    {"bnot", intlib_bnot},
    {"bxor", intlib_bxor},
    {"lt", intlib_lt},
    {"le", intlib_le},
    {"ult", intlib_ult},
    {"ule", intlib_ule},
    {"lshift", intlib_lshift},
    {"rshift", intlib_rshift},
    {"arshift", intlib_arshift},
    {"lrotate", intlib_lrotate},
    {"rrotate", intlib_rrotate},
    {"extract", intlib_extract},
    {"replace", intlib_replace},
    {"btest", intlib_btest},
    {"countrz", intlib_countrz},
    {"countlz", intlib_countlz},
    {"bswap", intlib_bswap},
    {NULL, NULL},
};

int luaopen_integer(lua_State* L)
{
    luaL_register(L, LUA_INTEGERLIBNAME, integerlib);
    return 1;
}
