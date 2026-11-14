#pragma once
// Polyphonic bytebeat voice engine. Independent of JUCE so it can be unit-tested.
//
// Each voice runs its own time counter `t`. The counter advances at `rate` Hz
// (at middle C) and is scaled by the note's pitch, so playing higher notes
// plays the formula faster - the same idea as classic chip/bytebeat synths.
// The formula is evaluated only when `t` changes (sample-and-hold), which
// gives the crunchy 8-bit character.

#include "FormulaEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace bb
{
struct SynthParams
{
    float rate = 8000.0f;   // Hz of the `t` counter at note 60 (C4)
    int   octave = 0;       // -3..+3
    float coarse = 0.0f;    // semitones
    float fine = 0.0f;      // cents
    int   crushBits = 8;    // 1..8 output bits
    float attack = 0.002f;  // seconds
    float release = 0.15f;  // seconds
    float volume = 0.7f;    // 0..1
};

class SynthCore
{
public:
    static constexpr int kNumVoices = 8;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        allNotesOff();
    }

    // The pointer must stay valid for as long as render() may be called with it.
    void setFormula (const Formula* f) noexcept { formula = f; }

    void noteOn (int note, int velocity) noexcept
    {
        int idx = -1;

        for (int i = 0; i < kNumVoices; ++i)
            if (voices[(size_t) i].active && ! voices[(size_t) i].releasing && voices[(size_t) i].note == note)
                idx = i;

        if (idx < 0)
            for (int i = 0; i < kNumVoices; ++i)
                if (! voices[(size_t) i].active) { idx = i; break; }

        if (idx < 0)
        {
            idx = 0;
            for (int i = 1; i < kNumVoices; ++i)
                if (voices[(size_t) i].age < voices[(size_t) idx].age)
                    idx = i;
        }

        auto& v = voices[(size_t) idx];
        const bool wasActive = v.active;
        v.active = true;
        v.releasing = false;
        v.note = note;
        v.vel = (float) std::clamp (velocity, 1, 127) / 127.0f;
        v.phase = 0.0;
        v.lastT = -1;
        v.sample = 0.0f;
        v.age = ++ageCounter;
        if (! wasActive)
            v.env = 0.0f;
    }

    void noteOff (int note) noexcept
    {
        for (auto& v : voices)
            if (v.active && ! v.releasing && v.note == note)
                v.releasing = true;
    }

    void allNotesOff() noexcept
    {
        for (auto& v : voices)
            v = Voice {};
    }

    // Adds `numSamples` mono samples to `out`.
    void render (float* out, int numSamples, const SynthParams& p) noexcept
    {
        if (formula == nullptr || ! formula->isValid())
            return;

        const float atkInc = p.attack > 0.0005f ? 1.0f / (p.attack * (float) sampleRate) : 1.0f;
        const float relInc = p.release > 0.0005f ? 1.0f / (p.release * (float) sampleRate) : 1.0f;
        const int bits = std::clamp (p.crushBits, 1, 8);
        const int mask = (0xFF << (8 - bits)) & 0xFF;
        const float gain = 0.3f * p.volume;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            const double semis = (double) (v.note - 60) + 12.0 * p.octave + (double) p.coarse + (double) p.fine / 100.0;
            const double step = (double) p.rate * std::pow (2.0, semis / 12.0) / sampleRate;

            Vars vars;
            vars.n = v.note;
            vars.v = (int64_t) std::lround (v.vel * 127.0f);
            vars.f = (int64_t) std::lround (440.0 * std::pow (2.0, (v.note - 69) / 12.0));

            for (int i = 0; i < numSamples; ++i)
            {
                if (v.releasing)
                {
                    v.env -= relInc;
                    if (v.env <= 0.0f)
                    {
                        v = Voice {};
                        break;
                    }
                }
                else if (v.env < 1.0f)
                {
                    v.env = std::min (1.0f, v.env + atkInc);
                }

                v.phase += step;
                const int64_t t = (int64_t) v.phase;
                if (t != v.lastT)
                {
                    v.lastT = t;
                    vars.t = t;
                    const int byte = (int) (formula->eval (vars) & 0xFF) & mask;
                    v.sample = ((float) byte - 128.0f) / 128.0f;
                }

                out[i] += v.sample * v.env * v.vel * gain;
            }
        }
    }

    int activeVoices() const noexcept
    {
        int n = 0;
        for (auto& v : voices)
            n += v.active ? 1 : 0;
        return n;
    }

private:
    struct Voice
    {
        bool active = false;
        bool releasing = false;
        int note = 0;
        float vel = 0.0f;
        double phase = 0.0;
        int64_t lastT = -1;
        float sample = 0.0f;
        float env = 0.0f;
        uint32_t age = 0;
    };

    std::array<Voice, kNumVoices> voices {};
    const Formula* formula = nullptr;
    double sampleRate = 44100.0;
    uint32_t ageCounter = 0;
};
} // namespace bb
