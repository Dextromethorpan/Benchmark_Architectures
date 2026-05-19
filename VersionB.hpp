#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <cstring>
#include <string>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────
// VERSION B — System Class Architecture
// AudioSystem + MemoryPool + Factory as a Service
//
// • MemoryPool owns all buffers — allocated ONCE at startup
// • Factory lives inside AudioSystem — exposed as a service
// • AudioSystem passed by pointer — no global, no singleton
// • Zero heap allocations during steady-state processing
//
// This is the AFTER picture.
// ─────────────────────────────────────────────────────────────

namespace VersionB {

// ── Memory Pool ──────────────────────────────────────────────
// Allocates one large contiguous block at startup.
// Hands out fixed-size sub-blocks during processing.
// No malloc/free calls during steady-state.
class MemoryPool {
public:
    explicit MemoryPool(std::size_t blockSize, std::size_t numBlocks)
        : mBlockSize(blockSize)
    {
        // ONE allocation at startup — never again during processing
        mStorage.resize(blockSize * numBlocks);
        for (std::size_t i = 0; i < numBlocks; ++i)
            mFreeList.push_back(mStorage.data() + i * blockSize);
    }

    float* acquire() {
        if (mFreeList.empty())
            throw std::runtime_error("MemoryPool exhausted");
        float* p = mFreeList.back();
        mFreeList.pop_back();
        return p;                             // ← NO malloc
    }

    void release(float* p) {
        mFreeList.push_back(p);               // ← NO free
    }

    std::size_t blockSize() const { return mBlockSize; }

private:
    std::size_t          mBlockSize;
    std::vector<float>   mStorage;            // single allocation
    std::vector<float*>  mFreeList;
};

// ── Abstract Product: IEffect ────────────────────────────────
class IEffect {
public:
    virtual ~IEffect() = default;
    virtual void process(float* buffer, std::size_t n) = 0;
    virtual const char* name() const = 0;
};

// ── Concrete Products ────────────────────────────────────────

// GainEffect — receives a scratch buffer from the pool at construction.
// process() uses the pre-allocated buffer — zero allocation per block.
class GainEffect final : public IEffect {
public:
    GainEffect(float gainDb, float* scratchBuffer, std::size_t bufSize)
        : mGain(std::pow(10.f, gainDb / 20.f))
        , mScratch(scratchBuffer)             // ← pool memory, not new
        , mBufSize(bufSize) {}

    void process(float* buffer, std::size_t n) override {
        std::size_t count = std::min(n, mBufSize);
        // Use pre-allocated scratch — ZERO heap allocation
        std::memcpy(mScratch, buffer, count * sizeof(float));
        for (std::size_t i = 0; i < count; ++i)
            buffer[i] = mScratch[i] * mGain;
    }

    const char* name() const override { return "VersionB::GainEffect"; }

private:
    float       mGain;
    float*      mScratch;                     // points into pool — not owned
    std::size_t mBufSize;
};

// BandPassEffect — also uses pool memory
class BandPassEffect final : public IEffect {
public:
    BandPassEffect(float centreHz, float sampleRate,
                   float* scratchBuffer, std::size_t bufSize)
        : mCentre(centreHz), mSampleRate(sampleRate)
        , mScratch(scratchBuffer), mBufSize(bufSize) {}

    void process(float* buffer, std::size_t n) override {
        std::size_t count = std::min(n, mBufSize);
        // Use pre-allocated scratch — ZERO heap allocation
        for (std::size_t i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / mSampleRate;
            mScratch[i] = buffer[i] * std::sin(2.f * 3.14159f * mCentre * t);
        }
        std::memcpy(buffer, mScratch, count * sizeof(float));
    }

    const char* name() const override { return "VersionB::BandPassEffect"; }

private:
    float       mCentre, mSampleRate;
    float*      mScratch;
    std::size_t mBufSize;
};

// ── Abstract Creator: IEffectFactory ────────────────────────
class IEffectFactory {
public:
    virtual ~IEffectFactory() = default;
    virtual std::unique_ptr<IEffect> createEffect(MemoryPool& pool) const = 0;
    virtual const char* effectName() const = 0;
};

// ── Concrete Creators ────────────────────────────────────────
class GainEffectFactory final : public IEffectFactory {
public:
    explicit GainEffectFactory(float gainDb) : mGainDb(gainDb) {}

    std::unique_ptr<IEffect> createEffect(MemoryPool& pool) const override {
        float* scratch = pool.acquire();      // uses system's pool
        return std::make_unique<GainEffect>(mGainDb, scratch, pool.blockSize());
    }

    const char* effectName() const override { return "gain"; }

private:
    float mGainDb;
};

class BandPassEffectFactory final : public IEffectFactory {
public:
    BandPassEffectFactory(float centreHz, float sampleRate)
        : mCentre(centreHz), mSampleRate(sampleRate) {}

    std::unique_ptr<IEffect> createEffect(MemoryPool& pool) const override {
        float* scratch = pool.acquire();      // uses system's pool
        return std::make_unique<BandPassEffect>(
            mCentre, mSampleRate, scratch, pool.blockSize());
    }

    const char* effectName() const override { return "bandpass"; }

private:
    float mCentre, mSampleRate;
};

// ── Effect Chain ─────────────────────────────────────────────
class EffectChain {
public:
    void addEffect(std::unique_ptr<IEffect> fx) {
        mEffects.push_back(std::move(fx));
    }

    void process(float* buffer, std::size_t n) {
        for (auto& fx : mEffects)
            fx->process(buffer, n);
    }

    std::size_t size() const { return mEffects.size(); }

private:
    std::vector<std::unique_ptr<IEffect>> mEffects;
};

// ── AudioSystem — The System Class ───────────────────────
// • Owns MemoryPool — one allocation at startup
// • Owns EffectChain
// • Exposes factory as a service (registerFactory / addEffect)
// • Passed by pointer — no global, no singleton
class AudioSystem {
public:
    explicit AudioSystem(float sampleRate,
                         std::size_t blockSize = 4096,
                         std::size_t numBlocks = 16)
        : mSampleRate(sampleRate)
        , mPool(std::make_unique<MemoryPool>(blockSize, numBlocks))
        , mChain(std::make_unique<EffectChain>())
    {}

    // ── Factory service ──────────────────────────────────────
    void registerFactory(std::unique_ptr<IEffectFactory> factory) {
        mFactories[factory->effectName()] = std::move(factory);
    }

    void addEffect(const std::string& name) {
        auto it = mFactories.find(name);
        if (it == mFactories.end())
            throw std::runtime_error("Unknown effect: " + name);
        // Factory uses the system's own pool — no external malloc
        mChain->addEffect(it->second->createEffect(*mPool));
    }

    // ── Processing ───────────────────────────────────────────
    void processBlock(float* buffer, std::size_t n) {
        // Zero allocations — pool memory already in use
        mChain->process(buffer, n);
    }

    float       sampleRate() const { return mSampleRate; }
    MemoryPool& pool()             { return *mPool; }

private:
    float       mSampleRate;
    std::unique_ptr<MemoryPool>   mPool;
    std::unique_ptr<EffectChain>  mChain;
    std::unordered_map<std::string,
        std::unique_ptr<IEffectFactory>> mFactories;
};

} // namespace VersionB