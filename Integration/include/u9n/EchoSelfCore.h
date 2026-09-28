// EchoSelfCore.h — C++ port of the 9cog/EchoSelf-48M cognitive services.
//
//   ToroidalCognitiveService (TS) → Toroid        : DTE (phase 0) ⟷ Marduk (phase π) on a torus
//   hypergraph.scm            (Scheme) → Hypergraph: salience + adaptive-attention
//   EchoSpaceService          (TS) → EchoSpace     : cosine-similarity vector memory
//   DeepTreeEchoService       (TS) → ResonanceLog  : cycle metrics + improvement trend
//   NanEcho 48M (nanoGPT)         → NanEchoSpec    : architecture descriptor (weights not bundled)
#pragma once

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
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

// Vector memory with optional consolidation (whitening).
//
// Raw cosine recall is dominated by the few directions every reservoir state shares
// (bias, input-driven common mode), so distinct episodes look alike. Consolidate()
// fits a shrunk PCA whitening on the *stored* keys only (Gram-matrix trick, O(M²·D)):
//     z_c = u_cᵀ (k − μ) / (σ_c + α·√(σ_max·σ_median))   (σ_c = √λ_c, per retained component)
// which flattens shared directions while shrinkage keeps noise-only directions from
// being blown up. Cues are projected with the same fitted map; they never influence it.
class EchoSpace {
public:
    enum class Kind { Episodic, Semantic, Procedural, Declarative };
    struct Entry { Eigen::VectorXd Raw, Key; int Label; Kind Type; double Salience; };

    double Shrinkage = 0.1;    // ridge = α·√(σ_max·σ_median); swept: 0.1 → probe σ=0.25 recall 1.00, σ=0.5 0.64

    void Store(const Eigen::VectorXd& key, int label, Kind k = Kind::Episodic, double salience = 1.0) {
        Entries.push_back({key, Project(key), label, k, salience});
    }

    // Fit the whitening map on current contents and re-project stored keys.
    // Returns false (no-op) with fewer than 2 entries or mismatched key sizes.
    bool Consolidate() {
        const int M = (int)Entries.size();
        if (M < 2) return false;
        const Eigen::Index D = Entries[0].Raw.size();
        Eigen::MatrixXd X(D, M);
        for (int i = 0; i < M; ++i) {
            if (Entries[i].Raw.size() != D) return false;
            X.col(i) = Entries[i].Raw;
        }
        Mean = X.rowwise().mean();
        X.colwise() -= Mean;
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(X.transpose() * X);
        const Eigen::VectorXd ev = es.eigenvalues();          // ascending
        const double top = std::sqrt(std::max(ev[M - 1], 1e-300));
        // Ridge scale = geometric mean of the largest and median component. Scaling to the
        // largest alone lets one dominant shared mode swamp every private direction (whitening
        // silently turns off); scaling to the median alone under-regularises steep reservoir
        // spectra and amplifies cue noise.
        std::vector<double> scs;
        for (int c = 0; c < M; ++c) {
            const double sc = std::sqrt(std::max(ev[c], 0.0));
            if (sc > 1e-6 * top) scs.push_back(sc);
        }
        if (scs.empty()) return false;
        std::nth_element(scs.begin(), scs.begin() + scs.size() / 2, scs.end());
        const double ridge = Shrinkage * std::sqrt(top * scs[scs.size() / 2]);
        Eigen::MatrixXd U = X * es.eigenvectors();             // D×M, column c has norm √ev_c
        // Centering makes the Gram matrix rank ≤ M−1; components at numerical zero carry
        // only round-off, which whitening would amplify without bound, so drop them.
        int kept = 0;
        for (int c = 0; c < M; ++c) {
            const double sc = std::sqrt(std::max(ev[c], 0.0));
            if (sc <= 1e-6 * top) { U.col(c).setZero(); continue; }
            U.col(c) /= sc * (sc + ridge);
            ++kept;
        }
        if (kept == 0) return false;
        Whitener = U.transpose();
        Whitened = true;
        for (auto& e : Entries) e.Key = Project(e.Raw);
        return true;
    }

    // Returns label of nearest entry by cosine (in whitened space once consolidated); -1 when empty.
    int Recall(const Eigen::VectorXd& q, double* sim = nullptr) const {
        const Eigen::VectorXd qn = Project(q);
        int best = -1; double bs = -2;
        for (auto& e : Entries) {
            const double s = e.Key.dot(qn) * (0.9 + 0.1 * e.Salience);
            if (s > bs) { bs = s; best = e.Label; }
        }
        if (sim) *sim = bs;
        return best;
    }
    size_t Size() const { return Entries.size(); }
    bool IsConsolidated() const { return Whitened; }

private:
    Eigen::VectorXd Project(const Eigen::VectorXd& k) const {
        if (!Whitened || k.size() != Mean.size()) return k.normalized();
        return (Whitener * (k - Mean)).normalized();
    }

    std::vector<Entry> Entries;
    Eigen::VectorXd Mean;
    Eigen::MatrixXd Whitener;
    bool Whitened = false;
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
