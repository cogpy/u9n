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
    struct Entry { Eigen::VectorXd Raw, Key; int Label; Kind Type; double Salience; long Stamp; };

    // Capacity 0 = unbounded. On overflow the entry with the lowest retention
    //     r = Salience · RetentionDecay^age   (age = stores since it was written)
    // is evicted, so salient episodes outlive mundane ones but still fade eventually.
    // RetentionDecay = 1 gives pure salience ranking; salience all equal gives FIFO.
    size_t Capacity = 0;
    double RetentionDecay = 0.998;   // half-life ≈ 346 stores (sweep in Integration/README.md)
    // Recall-driven reinforcement (RecallAndReinforce): a confident match gains salience
    // s ← s + ReinforceBoost·(1 − s) and is rehearsed (age reset), so memories that keep
    // getting used resist eviction. Confidence is how far the best match stands out from all
    // stored entries, z = (best − mean)/std over the cue's similarities. It is scale-free
    // (absolute cosine levels shift with whitening and key size) and, measured on agent
    // episodes, separates stored from evicted-episode cues far better than a similarity
    // cut-off or a best-vs-runner-up margin (sweep in Integration/README.md).
    // z cannot exceed √(n−1) for n entries, so small stores use 0.9·√(n−1) as the threshold.
    double ReinforceBoost = 0.2;
    double ReinforceMinZ  = 3.75;
    double Shrinkage = 0.1;    // ridge = α·√(σ_max·σ_median); swept: 0.1 → probe σ=0.25 recall 1.00, σ=0.5 0.64

    void Store(const Eigen::VectorXd& key, int label, Kind k = Kind::Episodic, double salience = 1.0) {
        if (Capacity > 0 && Entries.size() >= Capacity) Entries.erase(Entries.begin() + LeastRetained());
        Entries.push_back({key, Project(key), label, k, salience, Clock++});
    }

    double Retention(size_t i) const {
        return Entries[i].Salience * std::pow(RetentionDecay, double(Clock - Entries[i].Stamp));
    }
    bool Contains(int label) const {
        for (auto& e : Entries) if (e.Label == label) return true;
        return false;
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
    // Pure lookup: never changes salience or age.
    int Recall(const Eigen::VectorXd& q, double* sim = nullptr) const {
        const int i = Nearest(q, sim);
        return i < 0 ? -1 : Entries[i].Label;
    }

    // Recall that strengthens what it retrieves (see ReinforceBoost / ReinforceMinZ).
    // Weak matches are returned but not reinforced, so a false recall cannot entrench itself.
    int RecallAndReinforce(const Eigen::VectorXd& q, double* sim = nullptr) {
        double s = 0;
        const int i = Nearest(q, &s);
        if (sim) *sim = s;
        if (i < 0) return -1;
        if (s > 0 && RecallConfidence(q) >= ConfidenceThreshold()) {
            Entry& e = Entries[i];
            e.Salience += ReinforceBoost * (1.0 - e.Salience);
            e.Stamp = Clock;
        }
        return Entries[i].Label;
    }

    // Cosine of cue q to every stored entry (in whitened space once consolidated), salience-weighted.
    std::vector<double> Similarities(const Eigen::VectorXd& q) const {
        const Eigen::VectorXd qn = Project(q);
        std::vector<double> out; out.reserve(Entries.size());
        for (auto& e : Entries) out.push_back(e.Key.dot(qn) * (0.9 + 0.1 * e.Salience));
        return out;
    }

    // z-score of the best match among all stored entries for cue q (0 with fewer than 2 entries).
    double RecallConfidence(const Eigen::VectorXd& q) const {
        const std::vector<double> v = Similarities(q);
        if (v.size() < 2) return 0.0;
        double mean = 0, best = -2;
        for (double x : v) { mean += x; best = std::max(best, x); }
        mean /= v.size();
        double var = 0;
        for (double x : v) var += (x - mean) * (x - mean);
        const double sd = std::sqrt(var / v.size());
        return sd > 1e-12 ? (best - mean) / sd : 0.0;
    }
    double ConfidenceThreshold() const {
        return std::min(ReinforceMinZ, 0.9 * std::sqrt(std::max<double>(0.0, double(Entries.size()) - 1.0)));
    }

    double SalienceOf(int label) const {
        for (auto& e : Entries) if (e.Label == label) return e.Salience;
        return 0.0;
    }
    size_t Size() const { return Entries.size(); }
    bool IsConsolidated() const { return Whitened; }

private:
    int Nearest(const Eigen::VectorXd& q, double* sim) const {
        const Eigen::VectorXd qn = Project(q);
        int best = -1; double bs = -2;
        for (int i = 0; i < (int)Entries.size(); ++i) {
            const double s = Entries[i].Key.dot(qn) * (0.9 + 0.1 * Entries[i].Salience);
            if (s > bs) { bs = s; best = i; }
        }
        if (sim) *sim = bs;
        return best;
    }

    size_t LeastRetained() const {
        size_t worst = 0;
        for (size_t i = 1; i < Entries.size(); ++i)
            if (Retention(i) < Retention(worst)) worst = i;   // ties keep the oldest (lowest index)
        return worst;
    }

    Eigen::VectorXd Project(const Eigen::VectorXd& k) const {
        if (!Whitened || k.size() != Mean.size()) return k.normalized();
        return (Whitener * (k - Mean)).normalized();
    }

    std::vector<Entry> Entries;
    Eigen::VectorXd Mean;
    Eigen::MatrixXd Whitener;
    bool Whitened = false;
    long Clock = 0;
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
