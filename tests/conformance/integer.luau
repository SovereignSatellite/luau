-- This file tests the integer type and integer library

-- integer literal parsing
do
    local a = 0i
    assert(type(a) == "integer")
    assert(typeof(a) == "integer")

    local b = 123i
    assert(type(b) == "integer")

    local c = 0xFFi
    assert(type(c) == "integer")

    local d = 0b1010i
    assert(type(d) == "integer")

    -- underscores in integer literals
    local e = 1_000_000i
    assert(type(e) == "integer")
end

-- integer equality
do
    assert(0i == 0i)
    assert(1i == 1i)
    assert(not (1i == 2i))
    assert(1i ~= 2i)
    assert(not (1i ~= 1i))

    -- integers with the same value are equal
    local a = 42i
    local b = 42i
    assert(a == b)

    -- different integers are not equal
    local c = 43i
    assert(a ~= c)
end

-- tostring
do
    assert(tostring(0i) == "0")
    assert(tostring(1i) == "1")
    assert(tostring(123i) == "123")
end

-- integer as table keys
do
    local t = {}
    t[1i] = "one"
    t[2i] = "two"
    assert(t[1i] == "one")
    assert(t[2i] == "two")

    -- different integer values give different keys
    t[3i] = "three"
    assert(t[1i] == "one")
    assert(t[3i] == "three")
end

-- integer.create
do
    local a = integer.create(0)
    assert(a == 0i)

    local b = integer.create(42)
    assert(b == 42i)

    local c = integer.create(-1)
    assert(type(c) == "integer")

    -- non-exact numbers return nil
    local d = integer.create(1.5)
    assert(d == nil)

    -- NaN returns nil
    local e = integer.create(0/0)
    assert(e == nil)
end

-- integer.fromstring
do
    assert(integer.fromstring("0") == 0i)
    assert(integer.fromstring("123") == 123i)
    assert(integer.fromstring("-123") == integer.neg(123i))
    assert(integer.fromstring("0xFF") == 255i)
    assert(integer.fromstring("0xff") == 255i)
    assert(integer.fromstring("FF", 16) == 255i)
    assert(integer.fromstring("1010", 2) == 10i)

    -- invalid strings return nil
    assert(integer.fromstring("abc") == nil)
    assert(integer.fromstring("") == nil)

    -- whitespace is trimmed
    assert(integer.fromstring("  42  ") == 42i)
end

-- integer.tostring
do
    assert(integer.tostring(0i) == "0")
    assert(integer.tostring(123i) == "123")
end

-- integer.tonumber
do
    assert(integer.tonumber(0i) == 0)
    assert(integer.tonumber(42i) == 42)
    assert(type(integer.tonumber(42i)) == "number")
end

-- arithmetic
do
    assert(integer.add(1i, 2i) == 3i)
    assert(integer.sub(5i, 3i) == 2i)
    assert(integer.mul(3i, 4i) == 12i)
    assert(integer.neg(5i) == integer.sub(0i, 5i))
end

-- division
do
    assert(integer.div(10i, 3i) == 3i) -- truncated division
    assert(integer.rem(10i, 3i) == 1i) -- truncated remainder
    assert(integer.idiv(10i, 3i) == 3i) -- floored division
    assert(integer.mod(10i, 3i) == 1i) -- floored modulus

    -- negative cases
    assert(integer.div(integer.neg(10i), 3i) == integer.neg(3i)) -- truncated toward zero
    assert(integer.idiv(integer.neg(10i), 3i) == integer.neg(4i)) -- floored

    -- division by zero errors
    local ok1, err1 = pcall(integer.div, 1i, 0i)
    assert(not ok1)

    local ok2, err2 = pcall(integer.rem, 1i, 0i)
    assert(not ok2)

    local ok3, err3 = pcall(integer.idiv, 1i, 0i)
    assert(not ok3)

    local ok4, err4 = pcall(integer.mod, 1i, 0i)
    assert(not ok4)
end

-- unsigned division
do
    assert(integer.udiv(10i, 3i) == 3i)
    assert(integer.urem(10i, 3i) == 1i)

    local ok, err = pcall(integer.udiv, 1i, 0i)
    assert(not ok)
end

-- comparison
do
    assert(integer.lt(1i, 2i) == true)
    assert(integer.lt(2i, 1i) == false)
    assert(integer.lt(1i, 1i) == false)

    assert(integer.le(1i, 2i) == true)
    assert(integer.le(1i, 1i) == true)
    assert(integer.le(2i, 1i) == false)

    assert(integer.ult(1i, 2i) == true)
    assert(integer.ule(1i, 1i) == true)
end

-- min, max, clamp
do
    assert(integer.min(1i, 2i) == 1i)
    assert(integer.max(1i, 2i) == 2i)
    assert(integer.clamp(5i, 1i, 10i) == 5i)
    assert(integer.clamp(0i, 1i, 10i) == 1i)
    assert(integer.clamp(20i, 1i, 10i) == 10i)
end

-- bitwise operations
do
    assert(integer.band(0xFFi, 0x0Fi) == 0x0Fi)
    assert(integer.bor(0xF0i, 0x0Fi) == 0xFFi)
    assert(integer.bxor(0xFFi, 0x0Fi) == 0xF0i)
    assert(integer.btest(0xFFi, 0x01i) == true)
    assert(integer.btest(0xF0i, 0x0Fi) == false)
end

-- shifts
do
    assert(integer.lshift(1i, 4i) == 16i)
    assert(integer.rshift(16i, 4i) == 1i)
    assert(integer.arshift(16i, 4i) == 1i)

    -- out of range shifts return 0
    assert(integer.lshift(1i, 64i) == 0i)
    assert(integer.rshift(1i, 64i) == 0i)
end

-- rotations
do
    assert(integer.lrotate(1i, 1i) == 2i)
    assert(integer.rrotate(2i, 1i) == 1i)
end

-- extract and replace
do
    -- extract the low 4 bits
    assert(integer.extract(0xABi, 0i, 4i) == 0xBi)
    -- extract bits 4..7
    assert(integer.extract(0xABi, 4i, 4i) == 0xAi)

    -- replace the low 4 bits
    assert(integer.replace(0xA0i, 0xBi, 0i, 4i) == 0xABi)
end

-- bit counting
do
    assert(integer.countlz(0i) == 64i)
    assert(integer.countrz(0i) == 64i)
    assert(integer.countlz(1i) == 63i)
    assert(integer.countrz(1i) == 0i)
    assert(integer.countrz(2i) == 1i)
end

-- bswap
do
    assert(integer.bswap(0i) == 0i)
    local v = integer.bswap(0x0102030405060708i)
    assert(v == 0x0807060504030201i)
end

-- bnot
do
    assert(integer.bnot(0i) == integer.neg(1i))
    assert(integer.band(integer.bnot(0xFFi), 0xFFFFi) == 0xFF00i)
end

-- string.format with integers
do
    assert(string.format("%d", 42i) == "42")
    assert(string.format("%x", 255i) == "ff")
    assert(string.format("%X", 255i) == "FF")
    assert(string.format("%o", 8i) == "10")
end

-- math.maxinteger and math.mininteger
do
    assert(type(math.maxinteger) == "integer")
    assert(type(math.mininteger) == "integer")
    assert(integer.lt(math.mininteger, math.maxinteger))
end

-- buffer.readinteger / buffer.writeinteger
do
    local buf = buffer.create(16)
    buffer.writeinteger(buf, 0, 42i)
    assert(buffer.readinteger(buf, 0) == 42i)

    buffer.writeinteger(buf, 8, integer.neg(1i))
    assert(buffer.readinteger(buf, 8) == integer.neg(1i))
end

-- string concatenation via tostring
do
    local s = tostring(42i) .. " hello"
    assert(s == "42 hello")
end

return "OK"
