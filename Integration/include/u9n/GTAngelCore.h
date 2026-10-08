// GTAngelCore.h — C++ port of GTAngelEcho `DteCognitiveCoreService` (C#).
//
// Source: cogpy/GTAngelEcho  GTAngel/Services/DteCognitiveCoreService.cs
//
//   1. ECAN attention   — 16 clusters × 32 neurons; STI ← STI·0.95 + softmax(4·rms)·0.05,
//                         LTI ← LTI·0.99 + STI·0.01 (Hebbian tracking)
//   2. MOSES miner      — cluster-centroid sign patterns, Jaccard dedupe, fitness EMA
//   3. Wout             — online ridge-SGD readout  [ActionCount × ReservoirSize]
//   4. Gated logits     — reservoir cluster c weighted by 1 + STI_c·ClusterCount
//   5. Thompson policy  — θ_a ~ Beta(α_a, β_a) · softmax(logits)_a
//   6. Coherence        — 0.35·attention + 0.30·pattern + 0.35·policy
//
// Constants match the C# service so the two sides of the UE5/WPF bridge agree.
#pragma once

#include <Eigen/Dense>
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <vector>

namespace u9n {

class GTAngelCore {
public:
    static constexpr int ReservoirSize = 512;
    static constexpr int ClusterCount  = 16;
    static constexpr int ClusterSize   = 32;
    static constexpr int ActionCount   = 18;
    static constexpr int MaxPatterns   = 256;
    static constexpr double StiDecay     = 0.95;
    static constexpr double HebbianDecay = 0.99;

    double LearningRate  = 1e-3;
    double RidgeLambda   = 1e-4;
    double JaccardThresh = 0.55;

    struct Pattern { std::array<bool, ClusterCount> Signature{}; double Fitness = 0.5; int Hits = 1; };
    struct Coherence { double Overall, Attention, Pattern, Policy; };

    explicit GTAngelCore(unsigned seed = 42) : Rng(seed) {
        Sti.fill(1.0 / ClusterCount);
        Lti.fill(0.0);
        Alpha.fill(1.0);
        Beta.fill(1.0);
        Wout = Eigen::MatrixXd::Zero(ActionCount, ReservoirSize);
        Wmeta = Eigen::VectorXd::Zero(2 * ReservoirSize + 1);
    }

    // ── 1. ECAN ────────────────────────────────────────────────────────────
    void UpdateAttention(const Eigen::VectorXd& x) {
        if (x.size() != ReservoirSize) return;
        std::array<double, ClusterCount> raw{}, ex{};
        double mx = -1e300, sum = 0;
        for (int c = 0; c < ClusterCount; ++c) {
            raw[c] = std::sqrt(x.segment(c * ClusterSize, ClusterSize).squaredNorm() / ClusterSize);
            mx = std::max(mx, raw[c]);
        }
        for (int c = 0; c < ClusterCount; ++c) { ex[c] = std::exp((raw[c] - mx) * 4.0); sum += ex[c]; }
        for (int c = 0; c < ClusterCount; ++c) {
            Sti[c] = Sti[c] * StiDecay + ex[c] / sum * (1.0 - StiDecay);
            Lti[c] = Lti[c] * HebbianDecay + Sti[c] * (1.0 - HebbianDecay);
        }
    }

    int TopCluster() const { return int(std::max_element(Sti.begin(), Sti.end()) - Sti.begin()); }

    double AttentionEntropy() const {
        double s = 0, h = 0;
        for (double v : Sti) s += v;
        for (double v : Sti) if (v > 0) { double p = v / s; h -= p * std::log(p); }
        return h;
    }

    // ── 2. MOSES pattern mining ────────────────────────────────────────────
    void MinePatterns(const Eigen::VectorXd& x, double reward) {
        if (x.size() != ReservoirSize) return;
        Pattern p;
        for (int c = 0; c < ClusterCount; ++c) p.Signature[c] = x.segment(c * ClusterSize, ClusterSize).mean() > 0;
        const double fit = std::clamp((reward + 1.0) / 2.0, 0.0, 1.0);
        for (auto& q : Patterns) {
            if (Jaccard(p.Signature, q.Signature) >= JaccardThresh) {
                q.Fitness = 0.9 * q.Fitness + 0.1 * fit;
                ++q.Hits;
                return;
            }
        }
        p.Fitness = fit;
        if ((int)Patterns.size() >= MaxPatterns) {
            auto worst = std::min_element(Patterns.begin(), Patterns.end(),
                [](const Pattern& a, const Pattern& b) { return a.Fitness < b.Fitness; });
            *worst = p;
        } else {
            Patterns.push_back(p);
        }
    }

    // ── 3. Online Wout ─────────────────────────────────────────────────────
    // Prospective self-model (u9n extension): a second-order readout over [x; x⊙x; 1] predicts
    // log of the readout's own squared error *before* the target is seen. Coherence only knows
    // the loss after the fact (an EMA); this asks "how wrong am I about to be, in this state?".
    double MetaRate = 0.2;
    double PredictErrorSq(const Eigen::VectorXd& x) const { return std::exp(Wmeta.dot(MetaFeatures(x))); }

    void TrainWout(const Eigen::VectorXd& x, const Eigen::VectorXd& target) {
        Eigen::VectorXd err = Wout * x - target;
        const double e2 = err.squaredNorm() / ActionCount;            // error before this update
        const Eigen::VectorXd f = MetaFeatures(x);
        Wmeta += MetaRate * (std::log(e2 + 1e-8) - Wmeta.dot(f)) * f / f.squaredNorm();
        const double loss = e2 + RidgeLambda * Wout.squaredNorm();
        Wout -= LearningRate * (err * x.transpose() + RidgeLambda * Wout);
        WoutLoss = WoutLoss * 0.99 + loss * 0.01;
        ++WoutSamples;
    }

    // ── 4. Attention-gated logits ──────────────────────────────────────────
    Eigen::VectorXd GatedLogits(const Eigen::VectorXd& x) const {
        Eigen::VectorXd w = x;
        for (int c = 0; c < ClusterCount; ++c)
            w.segment(c * ClusterSize, ClusterSize) *= 1.0 + Sti[c] * ClusterCount;
        return Wout * w;
    }

    // ── 5. Thompson sampling ───────────────────────────────────────────────
    int ThompsonSample(const Eigen::VectorXd& logits, int nActions = ActionCount) {
        Eigen::VectorXd p = (logits.head(nActions).array() - logits.head(nActions).maxCoeff()).exp();
        p /= p.sum();
        int best = 0; double bestScore = -1;
        for (int a = 0; a < nActions; ++a) {
            const double s = SampleBeta(Alpha[a], Beta[a]) * p[a];
            if (s > bestScore) { bestScore = s; best = a; }
        }
        return best;
    }

    // Change detection (u9n extension). With ChangeThreshold > 0, every update runs a
    // per-arm Page-Hinkley test on the gap between the arm's posterior mean and the reward
    // it just paid. Drift shrinks each step's evidence, so ordinary noise cancels out. When
    // the accumulated shortfall exceeds the threshold, the world has changed: every posterior
    // resets to the uniform prior so the sampler re-explores at once. Constant discounting
    // instead forgets all the time, which makes the sampler over-explore when nothing changes.
    double ChangeThreshold = 0.0;   // 0 → off
    double ChangeDrift     = 0.05;
    int ChangeResets() const { return Resets; }

    // Returns true when this update triggered a change-point reset.
    bool UpdateThompson(int a, double reward) {
        if (a < 0 || a >= ActionCount) return false;
        const double r = std::clamp((reward + 1.0) / 2.0, 0.0, 1.0);
        if (ChangeThreshold > 0) {
            Ph[a] += Alpha[a] / (Alpha[a] + Beta[a]) - r - ChangeDrift;
            PhMin[a] = std::min(PhMin[a], Ph[a]);
            if (Ph[a] - PhMin[a] > ChangeThreshold) {
                Alpha.fill(1.0); Beta.fill(1.0); Ph.fill(0.0); PhMin.fill(0.0);
                ++Resets;
                return true;
            }
        }
        Alpha[a] = std::min(Alpha[a] + r, 1000.0);
        Beta[a]  = std::min(Beta[a] + 1.0 - r, 1000.0);
        return false;
    }

    // Discounting lets the policy track non-stationary rewards (u9n extension;
    // C# service leaves this at 1.0 implicitly).
    void DiscountThompson(double gamma) {
        for (int a = 0; a < ActionCount; ++a) {
            Alpha[a] = 1.0 + (Alpha[a] - 1.0) * gamma;
            Beta[a]  = 1.0 + (Beta[a] - 1.0) * gamma;
        }
    }

    // ── 6. Coherence ───────────────────────────────────────────────────────
    Coherence ComputeCoherence() const {
        const double att = 1.0 - AttentionEntropy() / std::log((double)ClusterCount);
        double meanFit = 0;
        for (auto& p : Patterns) meanFit += p.Fitness;
        if (!Patterns.empty()) meanFit /= Patterns.size();
        const double pat = std::min(1.0, Patterns.size() / double(MaxPatterns) * 2.0) * meanFit;
        const double pol = std::max(0.0, 1.0 - std::min(1.0, WoutLoss / 0.1));
        return {0.35 * att + 0.30 * pat + 0.35 * pol, att, pat, pol};
    }

    const std::array<double, ClusterCount>& STI() const { return Sti; }
    const std::array<double, ClusterCount>& LTI() const { return Lti; }
    const std::vector<Pattern>& MinedPatterns() const { return Patterns; }
    double GetWoutLoss() const { return WoutLoss; }
    const Eigen::MatrixXd& Readout() const { return Wout; }

private:
    static Eigen::VectorXd MetaFeatures(const Eigen::VectorXd& x) {
        Eigen::VectorXd f(2 * x.size() + 1);
        f << x, x.array().square().matrix(), 1.0;
        return f;
    }
    Eigen::VectorXd Wmeta;
    static double Jaccard(const std::array<bool, ClusterCount>& a, const std::array<bool, ClusterCount>& b) {
        int inter = 0, uni = 0;
        for (int i = 0; i < ClusterCount; ++i) { inter += a[i] && b[i]; uni += a[i] || b[i]; }
        return uni == 0 ? 1.0 : double(inter) / uni;
    }
    double SampleBeta(double a, double b) {
        std::gamma_distribution<double> ga(a, 1.0), gb(b, 1.0);
        const double x = ga(Rng), y = gb(Rng);
        return x / (x + y + 1e-300);
    }

    std::mt19937 Rng;
    std::array<double, ClusterCount> Sti{}, Lti{};
    std::array<double, ActionCount> Alpha{}, Beta{};
    std::array<double, ActionCount> Ph{}, PhMin{};   // Page-Hinkley statistics per arm
    int Resets = 0;
    std::vector<Pattern> Patterns;
    Eigen::MatrixXd Wout;
    double WoutLoss = 1.0;
    long WoutSamples = 0;
};

} // namespace u9n
