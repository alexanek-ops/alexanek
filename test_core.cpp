// Standalone tests for the JUCE-free core. Build & run: see Tests/run_tests.sh
#include "../Source/SynthCore.h"

#include <cstdio>
#include <cstdlib>
#include <string>

static int failures = 0;

#define CHECK(cond)                                                              \
    do { if (! (cond)) { std::printf ("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)

static int64_t ev (const std::string& src, int64_t t, int64_t n = 60)
{
    bb::Formula f;
    std::string err;
    if (! f.compile (src, err))
    {
        std::printf ("compile error for '%s': %s\n", src.c_str(), err.c_str());
        ++failures;
        return -999999;
    }
    bb::Vars v;
    v.t = t;
    v.n = n;
    return f.eval (v);
}

static bool fails (const std::string& src, const char* expectedFragment = nullptr)
{
    bb::Formula f;
    std::string err;
    const bool ok = f.compile (src, err);
    if (ok) return false;
    if (expectedFragment != nullptr && err.find (expectedFragment) == std::string::npos)
    {
        std::printf ("error for '%s' was '%s', expected fragment '%s'\n", src.c_str(), err.c_str(), expectedFragment);
        return false;
    }
    return true;
}

int main()
{
    // --- precedence & basic ops (matches C) ---
    CHECK (ev ("t*2|t|t/2", 5) == (5 * 2 | 5 | 5 / 2));
    CHECK (ev ("1+2*3", 0) == 7);
    CHECK (ev ("(1+2)*3", 0) == 9);
    CHECK (ev ("t&t>>8", 0x1234) == (0x1234 & (0x1234 >> 8)));
    CHECK (ev ("t>>4&1", 0x30) == ((0x30 >> 4) & 1));
    CHECK (ev ("1|2^3&4", 0) == (1 | (2 ^ (3 & 4))));
    CHECK (ev ("1<2==1", 0) == ((1 < 2) == 1));
    CHECK (ev ("1<<2+1", 0) == (1 << 3));
    CHECK (ev ("10-3-2", 0) == 5);
    CHECK (ev ("100/10/5", 0) == 2);
    CHECK (ev ("17%5", 0) == 2);
    CHECK (ev ("-t", 7) == -7);
    CHECK (ev ("~0", 0) == -1);
    CHECK (ev ("!0+!5", 0) == 1);
    CHECK (ev ("0x10+0XfF", 0) == 16 + 255);
    CHECK (ev ("T*2", 3) == 6); // case-insensitive names

    // --- logic & ternary ---
    CHECK (ev ("t>3&&t<10", 5) == 1);
    CHECK (ev ("t>3&&t<10", 12) == 0);
    CHECK (ev ("t<3||t>10", 5) == 0);
    CHECK (ev ("t&128?255:0", 200) == 255);
    CHECK (ev ("t&128?255:0", 100) == 0);
    CHECK (ev ("t<1?10:t<2?20:30", 0) == 10);
    CHECK (ev ("t<1?10:t<2?20:30", 1) == 20);
    CHECK (ev ("t<1?10:t<2?20:30", 5) == 30);

    // --- variables & functions ---
    CHECK (ev ("n", 0, 72) == 72);
    CHECK (ev ("sin(0)", 0) == 0);
    CHECK (ev ("sin(64)", 0) == 127);
    CHECK (ev ("cos(0)", 0) == 127);
    CHECK (ev ("abs(0-5)", 0) == 5);
    CHECK (ev ("min(3,9)+max(3,9)", 0) == 12);
    CHECK (ev ("sqrt(144)", 0) == 12);

    // --- safety: no crashes on division by zero / odd shifts ---
    CHECK (ev ("5/0", 0) == 0);
    CHECK (ev ("5%0", 0) == 0);
    CHECK (ev ("1<<100", 0) == 0);
    CHECK (ev ("1>>100", 0) == 0);
    CHECK (ev ("1<<(0-1)", 0) == 0);
    CHECK (ev ("9223372036854775807*2", 0) == -2); // wraps, no UB

    // --- errors ---
    CHECK (fails (""));
    CHECK (fails ("   "));
    CHECK (fails ("t*", "end"));
    CHECK (fails ("(t+1", "expected ')'"));
    CHECK (fails ("t+1)", "unexpected"));
    CHECK (fails ("foo", "unknown name"));
    CHECK (fails ("foo(1)", "unknown function"));
    CHECK (fails ("sin()", "argument"));
    CHECK (fails ("min(1)", "argument"));
    CHECK (fails ("1.5*t", "decimals"));
    CHECK (fails ("t=3"));
    CHECK (fails ("t?1", "expected ':'"));
    CHECK (fails (std::string (500, '(') + "1" + std::string (500, ')'), "nested"));

    // a failed compile must leave the formula invalid & evaluating to 0
    {
        bb::Formula f;
        std::string err;
        CHECK (f.compile ("t+1", err));
        CHECK (! f.compile ("t+", err));
        CHECK (! f.isValid());
        CHECK (f.eval (bb::Vars {}) == 0);
    }

    // --- synth: pitch tracks the note ---
    {
        bb::Formula square;
        std::string err;
        CHECK (square.compile ("t&128?255:0", err)); // square wave, period = 256 ticks of t

        auto countEdges = [&] (int note, int octave)
        {
            bb::SynthCore core;
            core.prepare (48000.0);
            core.setFormula (&square);
            bb::SynthParams p;
            p.attack = 0.0f;
            p.rate = 8000.0f;
            p.octave = octave;
            std::vector<float> buf (48000, 0.0f);
            core.noteOn (note, 127);
            core.render (buf.data(), (int) buf.size(), p);
            int edges = 0;
            for (size_t i = 1; i < buf.size(); ++i)
                if ((buf[i] > 0.0f) != (buf[i - 1] > 0.0f))
                    ++edges;
            return edges;
        };

        const int e60 = countEdges (60, 0);   // 8000/256 = 31.25 Hz -> ~62.5 edges
        const int e72 = countEdges (72, 0);   // one octave up       -> ~125 edges
        const int e60o = countEdges (60, 1);  // octave param        -> ~125 edges
        std::printf ("edges: C4=%d C5=%d C4+oct=%d\n", e60, e72, e60o);
        CHECK (e60 >= 60 && e60 <= 65);
        CHECK (e72 >= 122 && e72 <= 128);
        CHECK (e60o >= 122 && e60o <= 128);
    }

    // --- synth: voices, release, stealing, crush ---
    {
        bb::Formula f;
        std::string err;
        CHECK (f.compile ("t*2|t|t/2", err));
        bb::SynthCore core;
        core.prepare (44100.0);
        core.setFormula (&f);
        bb::SynthParams p;
        p.release = 0.01f;
        std::vector<float> buf (4410, 0.0f);

        core.noteOn (60, 100);
        core.render (buf.data(), (int) buf.size(), p);
        float peak = 0.0f;
        for (float s : buf) peak = std::max (peak, std::abs (s));
        CHECK (peak > 0.01f);
        CHECK (peak <= 1.0f);
        CHECK (core.activeVoices() == 1);

        core.noteOff (60);
        std::fill (buf.begin(), buf.end(), 0.0f);
        core.render (buf.data(), (int) buf.size(), p); // 0.1 s > 10 ms release
        CHECK (core.activeVoices() == 0);

        for (int i = 0; i < 20; ++i)
            core.noteOn (40 + i, 100); // more notes than voices -> stealing, no crash
        CHECK (core.activeVoices() == bb::SynthCore::kNumVoices);

        // 1-bit crush: output only takes values {-1, 0}*gain pattern -> few distinct levels
        core.allNotesOff();
        p.crushBits = 1;
        p.attack = 0.0f;
        core.noteOn (60, 127);
        std::fill (buf.begin(), buf.end(), 0.0f);
        core.render (buf.data(), (int) buf.size(), p);
        std::vector<float> uniq;
        for (float s : buf)
            if (std::find (uniq.begin(), uniq.end(), s) == uniq.end() && uniq.size() < 10)
                uniq.push_back (s);
        CHECK (uniq.size() <= 3);

        // no formula / invalid formula -> silence, no crash
        bb::SynthCore c2;
        c2.prepare (44100.0);
        c2.noteOn (60, 100);
        std::fill (buf.begin(), buf.end(), 0.0f);
        c2.render (buf.data(), (int) buf.size(), p);
        for (float s : buf) CHECK (s == 0.0f);
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
