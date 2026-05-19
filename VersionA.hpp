#pragma once
#include <vector>
#include <memory>
#include <cmath>
#include <cstring>
#include <string>

// ─────────────────────────────────────────────────────────────
// VERSION A — Direct Allocation Style
// Simulates the architecture of the current Equalizer:
//
// • No memory pool — buffers allocated fresh each block
// • No factory — effects constructed directly with new
// • No system class — components wired together directly
// • Represents the direct allocation style
//
// This is the BEFORE picture.
// ─────────────────────────────────────────────────────────────

namespace VersionA {

// ── Effect interface ─────────────────────────────────────────
class IEffect {
public:
    virtual ~IEffect() = default;
    virtual void process(float* buffer, std::size_t n) = 0;
    virtual const char* name() const = 0;
};

// ── Concrete effects ─────────────────────────────────────────

// GainEffect — allocates a temporary working buffer each call
// This is the key difference from Version B:
// every process() call allocates and frees heap memory.
class GainEffect final : public IEffect {
public:
    explicit GainEffect(float gainDb)
        : mGain(std::pow(10.f, gainDb / 20.f)) {}

    void process(float* buffer, std::size_t n) override {
        // Simulates a common pattern in naive DSP code:
        // allocate a temporary buffer for intermediate work,
        // use it, then free it — every single block.
        float* temp = new float[n];          // ← ALLOC every block
        std::memcpy(temp, buffer, n * sizeof(float));
        for (std::size_t i = 0; i < n; ++i)
            buffer[i] = temp[i] * mGain;
        delete[] temp;                        // ← FREE every block
    }

    const char* name() const override { return "VersionA::GainEffect"; }

private:
    float mGain;
};

// BandPassEffect — also allocates per block
class BandPassEffect final : public IEffect {
public:
    BandPassEffect(float centreHz, float sampleRate)
        : mCentre(centreHz), mSampleRate(sampleRate) {}

    void process(float* buffer, std::size_t n) override {
        // Again: allocate scratch buffer per block
        float* scratch = new float[n];       // ← ALLOC every block
        for (std::size_t i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / mSampleRate;
            scratch[i] = buffer[i] * std::sin(2.f * 3.14159f * mCentre * t);
        }
        std::memcpy(buffer, scratch, n * sizeof(float));
        delete[] scratch;                    // ← FREE every block
    }

    const char* name() const override { return "VersionA::BandPassEffect"; }

private:
    float mCentre;
    float mSampleRate;
};

// ── Effect chain ─────────────────────────────────────────────
// Direct vector of raw pointers — no factory, no system class.
// Effects are created directly with new wherever needed.
class EffectChain {
public:
    // Effects constructed directly — no factory abstraction
    void addGain(float gainDb) {
        mEffects.push_back(new GainEffect(gainDb));   // ← direct new
    }

    void addBandPass(float centre, float sr) {
        mEffects.push_back(new BandPassEffect(centre, sr)); // ← direct new
    }

    void process(float* buffer, std::size_t n) {
        for (auto* fx : mEffects)
            fx->process(buffer, n);
    }

    ~EffectChain() {
        for (auto* fx : mEffects)
            delete fx;                               // ← manual cleanup
    }

    std::size_t size() const { return mEffects.size(); }

private:
    std::vector<IEffect*> mEffects;                  // raw pointers
};

// ── Equalizer — top level object ────────────────────────────
// No system class — just a direct object with an effect chain.
// Passed around or accessed globally in the original architecture.
class Equalizer {
public:
    explicit Equalizer(float sampleRate) : mSampleRate(sampleRate) {
        // Add two effects directly — no factory involved
        mChain.addGain(6.0f);
        mChain.addBandPass(1000.f, mSampleRate);
    }

    void processBlock(float* buffer, std::size_t n) {
        // No pool — the effects will allocate internally
        mChain.process(buffer, n);
    }

    float sampleRate() const { return mSampleRate; }

private:
    float       mSampleRate;
    EffectChain mChain;
};

} // namespace VersionA