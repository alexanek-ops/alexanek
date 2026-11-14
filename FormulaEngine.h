#pragma once
// Bytebeat-style formula engine: parses a C-like integer expression once,
// then evaluates it cheaply per sample. No external dependencies.
//
// Variables : t (time counter), n (MIDI note), v (velocity 0..127), f (note Hz)
// Operators : unary - + ! ~   binary * / %  + -  << >>  < > <= >=  == !=  &  ^  |  &&  ||   ?:
//             (C precedence, 64-bit integer arithmetic, wraps on overflow)
// Functions : sin(x) cos(x)  -> period 256, range -127..127
//             abs(x) min(a,b) max(a,b) sqrt(x)
// Division / modulo by zero yield 0.

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace bb
{
struct Vars
{
    int64_t t = 0;
    int64_t n = 60;
    int64_t v = 127;
    int64_t f = 261;
};

namespace detail
{
enum class Op : uint8_t
{
    Const, VarT, VarN, VarV, VarF,
    Neg, Not, BitNot,
    Add, Sub, Mul, Div, Mod,
    Shl, Shr,
    Lt, Gt, Le, Ge, Eq, Ne,
    And, Xor, Or, LAnd, LOr,
    Cond,
    Sin, Cos, Abs, Min, Max, Sqrt
};

struct Node
{
    Op op = Op::Const;
    int64_t k = 0;
    int a = -1, b = -1, c = -1;
};

struct ParseError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class Parser
{
public:
    Parser (const std::string& source, std::vector<Node>& nodesOut) : s (source), nodes (nodesOut) {}

    int parse()
    {
        skipWs();
        if (pos >= s.size())
            fail ("formula is empty");
        int r = parseTernary();
        skipWs();
        if (pos < s.size())
            fail (std::string ("unexpected '") + s[pos] + "'");
        return r;
    }

private:
    const std::string& s;
    std::vector<Node>& nodes;
    size_t pos = 0;
    int depth = 0;

    [[noreturn]] void fail (const std::string& msg) const
    {
        throw ParseError (msg + " (at position " + std::to_string (pos + 1) + ")");
    }

    int add (Op op, int a = -1, int b = -1, int c = -1, int64_t k = 0)
    {
        if (nodes.size() >= 4000)
            fail ("formula is too long");
        Node n;
        n.op = op; n.a = a; n.b = b; n.c = c; n.k = k;
        nodes.push_back (n);
        return (int) nodes.size() - 1;
    }

    void skipWs()
    {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
            ++pos;
    }

    char peek()
    {
        skipWs();
        return pos < s.size() ? s[pos] : '\0';
    }

    void expect (char c)
    {
        if (peek() != c)
            fail (std::string ("expected '") + c + "'");
        ++pos;
    }

    struct BinInfo { Op op; int prec; size_t len; bool ok; };

    BinInfo peekBinary()
    {
        skipWs();
        if (pos >= s.size())
            return { Op::Add, 0, 0, false };
        const char c = s[pos];
        const char d = pos + 1 < s.size() ? s[pos + 1] : '\0';
        switch (c)
        {
            case '|': return d == '|' ? BinInfo { Op::LOr, 1, 2, true } : BinInfo { Op::Or, 3, 1, true };
            case '&': return d == '&' ? BinInfo { Op::LAnd, 2, 2, true } : BinInfo { Op::And, 5, 1, true };
            case '^': return { Op::Xor, 4, 1, true };
            case '=': return d == '=' ? BinInfo { Op::Eq, 6, 2, true } : BinInfo { Op::Add, 0, 0, false };
            case '!': return d == '=' ? BinInfo { Op::Ne, 6, 2, true } : BinInfo { Op::Add, 0, 0, false };
            case '<':
                if (d == '<') return { Op::Shl, 8, 2, true };
                if (d == '=') return { Op::Le, 7, 2, true };
                return { Op::Lt, 7, 1, true };
            case '>':
                if (d == '>') return { Op::Shr, 8, 2, true };
                if (d == '=') return { Op::Ge, 7, 2, true };
                return { Op::Gt, 7, 1, true };
            case '+': return { Op::Add, 9, 1, true };
            case '-': return { Op::Sub, 9, 1, true };
            case '*': return { Op::Mul, 10, 1, true };
            case '/': return { Op::Div, 10, 1, true };
            case '%': return { Op::Mod, 10, 1, true };
            default:  return { Op::Add, 0, 0, false };
        }
    }

    int parseTernary()
    {
        if (++depth > 100) fail ("formula is nested too deeply");
        int cond = parseBinary (1);
        if (peek() == '?')
        {
            ++pos;
            int a = parseTernary();
            expect (':');
            int b = parseTernary();
            cond = add (Op::Cond, a, b, cond);
        }
        --depth;
        return cond;
    }

    int parseBinary (int minPrec)
    {
        int lhs = parseUnary();
        for (;;)
        {
            auto info = peekBinary();
            if (! info.ok || info.prec < minPrec)
                break;
            pos += info.len;
            int rhs = parseBinary (info.prec + 1);
            lhs = add (info.op, lhs, rhs);
        }
        return lhs;
    }

    int parseUnary()
    {
        if (++depth > 100) fail ("formula is nested too deeply");
        const char c = peek();
        int r;
        if (c == '-')      { ++pos; r = add (Op::Neg, parseUnary()); }
        else if (c == '+') { ++pos; r = parseUnary(); }
        else if (c == '!') { ++pos; r = add (Op::Not, parseUnary()); }
        else if (c == '~') { ++pos; r = add (Op::BitNot, parseUnary()); }
        else               { r = parsePrimary(); }
        --depth;
        return r;
    }

    static bool isIdentStart (char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    static bool isDigit (char c)      { return c >= '0' && c <= '9'; }
    static bool isHex (char c)        { return isDigit (c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

    int parsePrimary()
    {
        const char c = peek();

        if (c == '(')
        {
            ++pos;
            int r = parseTernary();
            expect (')');
            return r;
        }

        if (isDigit (c))
        {
            uint64_t value = 0;
            if (c == '0' && pos + 1 < s.size() && (s[pos + 1] == 'x' || s[pos + 1] == 'X'))
            {
                pos += 2;
                if (pos >= s.size() || ! isHex (s[pos])) fail ("bad hex number");
                while (pos < s.size() && isHex (s[pos]))
                {
                    const char h = s[pos++];
                    value = value * 16 + (uint64_t) (isDigit (h) ? h - '0' : (h | 0x20) - 'a' + 10);
                }
            }
            else
            {
                while (pos < s.size() && isDigit (s[pos]))
                    value = value * 10 + (uint64_t) (s[pos++] - '0');
            }
            if (pos < s.size() && (s[pos] == '.' || isIdentStart (s[pos])))
                fail ("decimals are not supported - use integer math, e.g. (t*3)/2");
            return add (Op::Const, -1, -1, -1, (int64_t) value);
        }

        if (isIdentStart (c))
        {
            std::string name;
            while (pos < s.size() && (isIdentStart (s[pos]) || isDigit (s[pos])))
                name += (char) std::tolower ((unsigned char) s[pos++]);

            if (peek() == '(')
            {
                ++pos;
                std::vector<int> args;
                if (peek() != ')')
                {
                    for (;;)
                    {
                        args.push_back (parseTernary());
                        if (peek() == ',') { ++pos; continue; }
                        break;
                    }
                }
                expect (')');

                auto need = [&] (size_t count)
                {
                    if (args.size() != count)
                        fail ("'" + name + "' takes " + std::to_string (count) + " argument(s)");
                };

                if (name == "sin")  { need (1); return add (Op::Sin, args[0]); }
                if (name == "cos")  { need (1); return add (Op::Cos, args[0]); }
                if (name == "abs")  { need (1); return add (Op::Abs, args[0]); }
                if (name == "sqrt") { need (1); return add (Op::Sqrt, args[0]); }
                if (name == "min")  { need (2); return add (Op::Min, args[0], args[1]); }
                if (name == "max")  { need (2); return add (Op::Max, args[0], args[1]); }
                fail ("unknown function '" + name + "'");
            }

            if (name == "t") return add (Op::VarT);
            if (name == "n") return add (Op::VarN);
            if (name == "v") return add (Op::VarV);
            if (name == "f") return add (Op::VarF);
            fail ("unknown name '" + name + "'");
        }

        if (c == '\0')
            fail ("unexpected end of formula");
        fail (std::string ("unexpected '") + c + "'");
    }
};
} // namespace detail

class Formula
{
public:
    // Returns true on success. On failure `error` holds a readable message and
    // the object is left invalid (isValid() == false).
    bool compile (const std::string& source, std::string& error)
    {
        nodes.clear();
        root = -1;
        try
        {
            detail::Parser parser (source, nodes);
            root = parser.parse();
            error.clear();
            return true;
        }
        catch (const detail::ParseError& e)
        {
            nodes.clear();
            root = -1;
            error = e.what();
            return false;
        }
    }

    bool isValid() const noexcept { return root >= 0; }

    int64_t eval (const Vars& vars) const noexcept
    {
        return root < 0 ? 0 : run (root, vars);
    }

private:
    std::vector<detail::Node> nodes;
    int root = -1;

    static uint64_t u (int64_t x) noexcept { return (uint64_t) x; }

    int64_t run (int i, const Vars& v) const noexcept
    {
        using detail::Op;
        const auto& n = nodes[(size_t) i];

        switch (n.op)
        {
            case Op::Const: return n.k;
            case Op::VarT:  return v.t;
            case Op::VarN:  return v.n;
            case Op::VarV:  return v.v;
            case Op::VarF:  return v.f;

            case Op::Neg:    return (int64_t) (0ull - u (run (n.a, v)));
            case Op::Not:    return run (n.a, v) == 0 ? 1 : 0;
            case Op::BitNot: return ~run (n.a, v);

            case Op::LAnd: return (run (n.a, v) != 0 && run (n.b, v) != 0) ? 1 : 0;
            case Op::LOr:  return (run (n.a, v) != 0 || run (n.b, v) != 0) ? 1 : 0;
            case Op::Cond: return run (n.c, v) != 0 ? run (n.a, v) : run (n.b, v);

            default: break;
        }

        if (n.op == Op::Sin || n.op == Op::Cos || n.op == Op::Abs || n.op == Op::Sqrt)
        {
            const int64_t x = run (n.a, v);
            switch (n.op)
            {
                case Op::Sin:  return (int64_t) std::lround (std::sin ((double) (x & 255) * 6.283185307179586 / 256.0) * 127.0);
                case Op::Cos:  return (int64_t) std::lround (std::cos ((double) (x & 255) * 6.283185307179586 / 256.0) * 127.0);
                case Op::Abs:  return x < 0 ? (int64_t) (0ull - u (x)) : x;
                case Op::Sqrt: return x <= 0 ? 0 : (int64_t) std::sqrt ((double) x);
                default: break;
            }
        }

        const int64_t a = run (n.a, v);
        const int64_t b = run (n.b, v);

        switch (n.op)
        {
            case Op::Add: return (int64_t) (u (a) + u (b));
            case Op::Sub: return (int64_t) (u (a) - u (b));
            case Op::Mul: return (int64_t) (u (a) * u (b));
            case Op::Div: return (b == 0 || (a == INT64_MIN && b == -1)) ? 0 : a / b;
            case Op::Mod: return (b == 0 || b == -1) ? 0 : a % b;
            case Op::Shl: return (b < 0 || b > 63) ? 0 : (int64_t) (u (a) << b);
            case Op::Shr: return (b < 0 || b > 63) ? (a < 0 ? -1 : 0) : (a >> b);
            case Op::Lt:  return a < b;
            case Op::Gt:  return a > b;
            case Op::Le:  return a <= b;
            case Op::Ge:  return a >= b;
            case Op::Eq:  return a == b;
            case Op::Ne:  return a != b;
            case Op::And: return a & b;
            case Op::Xor: return a ^ b;
            case Op::Or:  return a | b;
            case Op::Min: return a < b ? a : b;
            case Op::Max: return a > b ? a : b;
            default:      return 0;
        }
    }
};
} // namespace bb
