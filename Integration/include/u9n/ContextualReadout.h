// ContextualReadout.h — continual-learning readout bank (fixes catastrophic forgetting
// of the single online Wout in GTAngelCore).
//
// Context key  k(t) = normalize( EMA_λ( x(t) ∘ x(t) ) )   — per-neuron power spectrum proxy.
// Routing      h*   = argmax_h cos(k, P_h);  spawn new head when max cos < NoveltyThreshold.
// Learning     only W_{h*} takes the GTAngel ridge-SGD step; P_{h*} drifts slowly toward k.
//
// No task labels are needed: tasks separate because they drive the reservoir into
// different power signatures. Heads never overwrite each other, so old skills persist.
#pragma once

#include <Eigen/Dense>
#include <vector>

namespace u9n {

class ContextualReadout {
public:
    double LearningRate     = 2e-3;
    double RidgeLambda      = 1e-4;
    double KeyDecay         = 0.98;
    double NoveltyThreshold = 0.97;
    double PrototypeRate    = 1e-3;
    int    MaxHeads         = 32;

    ContextualReadout(int reservoirSize, int outputs) : N(reservoirSize), K(outputs) { ResetContext(); }

    // Clear the short-term context trace (e.g. at episode boundaries). Heads persist.
    void ResetContext() { Key = Eigen::VectorXd::Zero(N); Warm = 0; }

    // Feed one reservoir state; updates the context key and returns the active head.
    int Observe(const Eigen::VectorXd& x) {
        Key = KeyDecay * Key + (1.0 - KeyDecay) * x.cwiseProduct(x);
        ++Warm;
        Active = Route(false);
        return Active;
    }

    Eigen::VectorXd Predict(const Eigen::VectorXd& x) const {
        return Active < 0 ? Eigen::VectorXd::Zero(K) : Eigen::VectorXd(Heads[Active].W * x);
    }

    // Returns false (no update) until the context key has settled, so the first samples
    // of a new task are never written into the previous task's head.
    bool Train(const Eigen::VectorXd& x, const Eigen::VectorXd& target) {
        if (!Settled()) return false;
        Active = Route(true);
        Head& h = Heads[Active];
        const Eigen::VectorXd err = h.W * x - target;
        h.W -= LearningRate * (err * x.transpose() + RidgeLambda * h.W);
        h.P = ((1.0 - PrototypeRate) * h.P + PrototypeRate * Key.normalized()).normalized();
        return true;
    }

    bool Settled() const { return Warm > long(3.0 / (1.0 - KeyDecay)); }

    int HeadCount() const { return (int)Heads.size(); }
    int ActiveHead() const { return Active; }

private:
    struct Head { Eigen::VectorXd P; Eigen::MatrixXd W; };

    int Route(bool allowSpawn) {
        if (Key.squaredNorm() < 1e-18) return Heads.empty() ? -1 : 0;
        const Eigen::VectorXd k = Key.normalized();
        int best = -1; double bs = -2;
        for (int i = 0; i < (int)Heads.size(); ++i) {
            const double s = Heads[i].P.dot(k);
            if (s > bs) { bs = s; best = i; }
        }
        if (allowSpawn && (best < 0 || (bs < NoveltyThreshold && (int)Heads.size() < MaxHeads))) {
            Heads.push_back({k, best < 0 ? Eigen::MatrixXd::Zero(K, N) : Heads[best].W});  // warm-start from nearest
            best = (int)Heads.size() - 1;
        }
        return best;
    }

    int N, K;
    Eigen::VectorXd Key;
    long Warm = 0;
    int Active = -1;
    std::vector<Head> Heads;
};

} // namespace u9n
