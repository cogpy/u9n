// EchoSelfCore.h — C++ port of the 9cog/EchoSelf-48M cognitive services.
//
//   ToroidalCognitiveService (TS) → Toroid        : DTE (phase 0) ⟷ Marduk (phase π) on a torus
//   hypergraph.scm            (Scheme) → Hypergraph: salience + adaptive-attention
//   EchoSpaceService          (TS) → EchoSpace     : cosine-similarity vector memory
//   DeepTreeEchoService       (TS) → ResonanceLog  : cycle metrics + improvement trend
//   NanEcho 48M (nanoGPT)         → NanEchoSpec    : architecture descriptor (weights not bundled)
#pragma once

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

namespace u9n {

inline constexpr double kPi = 3.14159265358979323846;

// Dual hemispheres as coupled phase oscillators (Kuramoto pair).
// dθ_i/dt = ω_i + K sin(θ_j - θ_i - π)   — locks at anti-phase (DTE 0, Marduk π).
class Toroid {
public:
    double ThetaDTE = 0.0, ThetaMarduk = 2.0, OmegaDTE = 1.0, OmegaMarduk = 1.07, Coupling = 0.4;

    void Advance(double dt) {
        const double dD = OmegaDTE    + Coupling * std::sin(ThetaMarduk - ThetaDTE - kPi);
        const double dM = OmegaMarduk + Coupling * std::sin(ThetaDTE - ThetaMarduk - kPi);
        ThetaDTE    = std::fmod(ThetaDTE + dD * dt, 2 * kPi);
        ThetaMarduk = std::fmod(ThetaMarduk + dM * dt, 2 * kPi);
    }
    // 1 when exactly anti-phase, 0 when in-phase.
    double Coherence() const { return 0.5 * (1.0 - std::cos(ThetaMarduk - ThetaDTE)); }
    // Hemispheric weights: creative (DTE) vs analytical (Marduk).
    double DTEWeight() const    { return 0.5 * (1.0 + std::cos(ThetaDTE)); }
    double MardukWeight() const { return 0.5 * (1.0 + std::cos(ThetaMarduk)); }
    // Insight fires at the zero-crossing of the DTE/Marduk balance.
    bool InsightWindow() const { return std::abs(DTEWeight() - MardukWeight()) < 0.05; }
};

// hypergraph.scm: (+ 0.5 (* load 0.3) (- 0.2 activity))
inline double AdaptiveAttention(double load, double activity) {
    return std::clamp(0.5 + load * 0.3 + (0.2 - activity), 0.0, 1.0);
}

class EchoSpace {
public:
    enum class Kind { Episodic, Semantic, Procedural, Declarative };
    struct Entry { Eigen::VectorXd Key; int Label; Kind Type; double Salience; };

    void Store(const Eigen::VectorXd& key, int label, Kind k = Kind::Episodic, double salience = 1.0) {
        Entries.push_back({key.normalized(), label, k, salience});
    }
    // Returns label of nearest entry by cosine; -1 when empty.
    int Recall(const Eigen::VectorXd& q, double* sim = nullptr) const {
        const Eigen::VectorXd qn = q.normalized();
        int best = -1; double bs = -2;
        for (auto& e : Entries) {
            const double s = e.Key.dot(qn) * (0.9 + 0.1 * e.Salience);
            if (s > bs) { bs = s; best = e.Label; }
        }
        if (sim) *sim = bs;
        return best;
    }
    size_t Size() const { return Entries.size(); }

private:
    std::vector<Entry> Entries;
};

class ResonanceLog {
public:
    void Record(double score) { if (History.size() >= 64) History.pop_front(); History.push_back(score); }
    // Least-squares slope over the recorded window.
    double ImprovementTrend() const {
        const int n = (int)History.size();
        if (n < 2) return 0;
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (int i = 0; i < n; ++i) { sx += i; sy += History[i]; sxx += i * i; sxy += i * History[i]; }
        return (n * sxy - sx * sy) / (n * sxx - sx * sx);
    }
private:
    std::deque<double> History;
};

struct NanEchoSpec {
    int Layers = 8, Heads = 8, Embed = 512, Block = 1024, Vocab = 50257;
    long long Params() const {
        const long long d = Embed;
        return Vocab * d + (long long)Block * d + Layers * (12 * d * d + 13 * d) + 2 * d;
    }
    std::string Source = "9cog/EchoSelf-48M NanEcho/config/train_nanecho.py";
};

} // namespace u9n
