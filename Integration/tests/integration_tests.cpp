// integration_tests.cpp — dependency-free checks for the u9n integration layer.
#include "u9n/ContextualReadout.h"
#include "u9n/UnifiedEchoAgent.h"

#include <cstdio>
#include <cstdlib>

using namespace u9n;

static int Failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++Failures; } } while (0)

static void ReservoirSpectralRadius() {
    ReservoirConfig c; c.Size = 200;
    EchoReservoir r(c);
    Eigen::MatrixXd M = Eigen::MatrixXd::Zero(3, 3);
    M(0, 1) = -0.9; M(1, 0) = 0.9; M(2, 2) = 0.5;           // dominant complex pair |λ| = 0.9
    CHECK(std::abs(EchoReservoir::SpectralRadiusOf(M) - 0.9) < 0.02);
    for (int t = 0; t < 100; ++t) r.Step(0.5);
    CHECK(r.State().allFinite());
    CHECK(r.State().cwiseAbs().maxCoeff() <= 1.0);
}

static void EcanFocusesSalientCluster() {
    GTAngelCore core;
    Eigen::VectorXd x = Eigen::VectorXd::Constant(GTAngelCore::ReservoirSize, 0.1);
    x.segment(5 * GTAngelCore::ClusterSize, GTAngelCore::ClusterSize).setConstant(0.9);
    for (int i = 0; i < 50; ++i) core.UpdateAttention(x);
    CHECK(core.TopCluster() == 5);
    double s = 0; for (double v : core.STI()) s += v;
    CHECK(std::abs(s - 1.0) < 1e-6);   // STI budget conserved
}

static void ThompsonPrefersRewardedArm() {
    GTAngelCore core;
    Eigen::VectorXd flat = Eigen::VectorXd::Zero(GTAngelCore::ActionCount);
    for (int i = 0; i < 200; ++i) { core.UpdateThompson(3, 1.0); core.UpdateThompson(4, -1.0); }
    int hits = 0;
    for (int i = 0; i < 100; ++i) hits += core.ThompsonSample(flat, 5) == 3;
    CHECK(hits > 60);
}

static void MosesDeduplicates() {
    GTAngelCore core;
    Eigen::VectorXd x = Eigen::VectorXd::Constant(GTAngelCore::ReservoirSize, 0.3);
    for (int i = 0; i < 10; ++i) core.MinePatterns(x, 1.0);
    CHECK(core.MinedPatterns().size() == 1);
    CHECK(core.MinedPatterns()[0].Hits == 10);
}

static void ToroidLocksAntiPhase() {
    Toroid t;
    for (int i = 0; i < 4000; ++i) t.Advance(0.05);
    CHECK(t.Coherence() > 0.9);
}

static void AdaptiveAttentionMatchesScheme() {
    // (+ 0.5 (* 0.5 0.3) (- 0.2 0.1)) = 0.75
    CHECK(std::abs(AdaptiveAttention(0.5, 0.1) - 0.75) < 1e-12);
}

static void EchoSpaceRecall() {
    EchoSpace m;
    m.Store(Eigen::Vector3d(1, 0, 0), 1);
    m.Store(Eigen::Vector3d(0, 1, 0), 2);
    CHECK(m.Recall(Eigen::Vector3d(0.1, 0.9, 0)) == 2);
}

// Mirrors the reservoir failure mode: every key carries a large component in a shared
// low-rank subspace with fresh random coefficients on each presentation, plus a small
// private signature. Raw cosine is swamped by the shared part; consolidation must fix it.
static void EchoSpaceConsolidationSeparatesSharedMode() {
    std::mt19937 rng(5); std::normal_distribution<double> N(0, 1);
    const int D = 64, M = 40, R = 3;
    const Eigen::MatrixXd shared = Eigen::MatrixXd::NullaryExpr(D, R, [&] { return N(rng); });
    auto sharedPart = [&] { return Eigen::VectorXd(shared * Eigen::VectorXd::NullaryExpr(R, [&] { return 5.0 * N(rng); })); };
    std::vector<Eigen::VectorXd> priv(M);
    EchoSpace raw, cons;
    for (int i = 0; i < M; ++i) {
        priv[i] = Eigen::VectorXd::NullaryExpr(D, [&] { return N(rng); });
        const Eigen::VectorXd k = sharedPart() + priv[i];
        raw.Store(k, i); cons.Store(k, i);
    }
    CHECK(cons.Consolidate());
    int okRaw = 0, okCons = 0;
    for (int i = 0; i < M; ++i) {
        const Eigen::VectorXd q = sharedPart() + priv[i] + 0.3 * Eigen::VectorXd::NullaryExpr(D, [&] { return N(rng); });
        okRaw += raw.Recall(q) == i; okCons += cons.Recall(q) == i;
    }
    CHECK(okCons >= M - 2);
    CHECK(okCons > okRaw);
}

static void KeyStoredAfterConsolidationOutsideSpan() {
    // A key orthogonal to the fitted span must stay reachable before the next consolidation.
    EchoSpace m;
    m.Store(Eigen::Vector3d(1, 0, 0), 0);
    m.Store(Eigen::Vector3d(-1, 0, 0), 1);
    CHECK(m.Consolidate());
    m.Store(Eigen::Vector3d(0, 0, 1), 2);
    CHECK(m.Recall(Eigen::Vector3d(0, 0, 1)) == 2);
    CHECK(m.Recall(Eigen::Vector3d(1, 0, 0)) == 0);
}

static void TrajectoryKeyShape() {
    ReservoirConfig c; c.Size = 16;
    EchoReservoir r(c);
    CHECK(r.EncodeTrajectory(std::vector<double>(10, 0.1)).size() == 160);
    CHECK(r.EncodeTrajectory(std::vector<double>(10, 0.1), 4).size() == 48);   // t=4,8 and last
    CHECK(r.EncodeTrajectory(std::vector<double>(10, 0.1), 0).size() == 160);  // stride 0 → every step
}

static void NanEchoParamCount() {
    NanEchoSpec s;
    CHECK(s.Params() > 45'000'000 && s.Params() < 55'000'000);
}

static void UnifiedCycleStreams() {
    UnifiedEchoAgent a;
    int acts = 0;
    for (int i = 0; i < 24; ++i) acts += a.Tick(Eigen::VectorXd::Constant(1, 0.2), 0.0) >= 0;
    CHECK(acts == 8);               // Acting stream = steps 2,5,8,11 per cycle
    CHECK(a.CurrentStep() == 12);
}

static void ContextualReadoutRetainsOldTask() {
    ReservoirConfig c; c.Size = 128;
    EchoReservoir r(c);
    ContextualReadout bank(c.Size, 1);
    auto run = [&](double f, int steps, bool train) {
        r.Reset(); bank.ResetContext();
        double se = 0, sv = 0;
        for (int t = 0; t < steps; ++t) {
            r.Step(std::sin(f * t));
            bank.Observe(r.State());
            const double y = std::sin(f * (t + 3));
            if (train) bank.Train(r.State(), Eigen::VectorXd::Constant(1, y));
            else if (t > 200) { se += std::pow(bank.Predict(r.State())[0] - y, 2); sv += y * y; }
        }
        return train ? 0.0 : std::sqrt(se / sv);
    };
    run(0.2, 4000, true);
    const double before = run(0.2, 600, false);
    run(0.05, 4000, true);
    const double after = run(0.2, 600, false);
    CHECK(bank.HeadCount() >= 2);
    CHECK(after < before + 0.05);      // task A survives learning task B
}

static void EchoSpaceSalienceWeightedEviction() {
    EchoSpace m;
    m.Capacity = 3;
    m.RetentionDecay = 1.0;                      // pure salience ranking
    m.Store(Eigen::Vector3d(1, 0, 0), 0, EchoSpace::Kind::Episodic, 1.0);
    m.Store(Eigen::Vector3d(0, 1, 0), 1, EchoSpace::Kind::Episodic, 0.1);
    m.Store(Eigen::Vector3d(0, 0, 1), 2, EchoSpace::Kind::Episodic, 1.0);
    m.Store(Eigen::Vector3d(1, 1, 0), 3, EchoSpace::Kind::Episodic, 0.5);
    CHECK(m.Size() == 3);
    CHECK(!m.Contains(1));                        // lowest salience evicted, not the oldest
    CHECK(m.Contains(0) && m.Contains(2) && m.Contains(3));

    // With decay, an old salient entry eventually loses to fresh mundane ones.
    EchoSpace d;
    d.Capacity = 2;
    d.RetentionDecay = 0.5;
    d.Store(Eigen::Vector3d(1, 0, 0), 0, EchoSpace::Kind::Episodic, 1.0);
    for (int i = 1; i <= 6; ++i) d.Store(Eigen::Vector3d(0, 1, double(i)), i, EchoSpace::Kind::Episodic, 0.3);
    CHECK(!d.Contains(0));

    // Equal salience degrades to FIFO.
    EchoSpace f;
    f.Capacity = 2;
    for (int i = 0; i < 3; ++i) f.Store(Eigen::Vector3d(1, double(i), 0), i);
    CHECK(!f.Contains(0) && f.Contains(1) && f.Contains(2));
}

static void RecallReinforcesSalience() {
    // A low-salience entry that keeps being recalled outlives a higher-salience one that is not.
    EchoSpace m;
    m.Capacity = 3;
    m.RetentionDecay = 0.9;
    m.Store(Eigen::Vector3d(1, 0, 0), 0, EchoSpace::Kind::Episodic, 0.2);   // used
    m.Store(Eigen::Vector3d(0, 1, 0), 1, EchoSpace::Kind::Episodic, 0.6);   // never used
    for (int i = 2; i < 10; ++i) {
        CHECK(m.RecallAndReinforce(Eigen::Vector3d(1, 0.05, 0)) == 0);
        m.Store(Eigen::Vector3d(0, 0.2 * i, 1), i, EchoSpace::Kind::Episodic, 0.5);
    }
    CHECK(m.Contains(0));
    CHECK(!m.Contains(1));
    CHECK(m.SalienceOf(0) > 0.8);                 // 0.2 → ~0.87 after 8 boosts of 0.2

    // Weak matches are not reinforced; pure Recall never changes salience.
    EchoSpace w;
    w.Store(Eigen::Vector3d(1, 0, 0), 7, EchoSpace::Kind::Episodic, 0.4);
    w.Store(Eigen::Vector3d(0, 1, 0), 8, EchoSpace::Kind::Episodic, 0.4);
    w.RecallAndReinforce(Eigen::Vector3d(-1, 0, -0.1));   // cosine < 0 with everything
    w.Recall(Eigen::Vector3d(1, 0, 0));
    CHECK(std::abs(w.SalienceOf(7) - 0.4) < 1e-12 && std::abs(w.SalienceOf(8) - 0.4) < 1e-12);
}

static void AgentConsolidatesEpisodicMemory() {
    UnifiedEchoAgent a(1, 7, /*memoryCapacity=*/64);
    a.ConsolidateEvery = 16;
    std::mt19937 rng(3); std::normal_distribution<double> N(0, 1);
    Eigen::VectorXd key; int label = -1;
    for (int t = 0; t < 12 * 120; ++t) {
        a.Tick(Eigen::VectorXd::Constant(1, 0.5 * N(rng)), 0.0);
        if (a.CurrentStep() == 12 && a.Torus.Coherence() > 0.5) { key = a.LastEpisodeKey(); label = (int)a.CyclesCompleted() - 1; }
    }
    CHECK(a.Memory.Size() <= 64);
    CHECK(a.Consolidations() >= 1);
    CHECK(a.Memory.IsConsolidated());
    CHECK(label >= 0);
    // A lightly corrupted copy of the most recent stored episode must come back to it.
    Eigen::VectorXd cue = key + 0.02 * Eigen::VectorXd::NullaryExpr(key.size(), [&] { return N(rng); });
    CHECK(a.RecallEpisode(cue) == label);
}

static void AugmentStacksSquares() {
    Eigen::MatrixXd S(2, 3); S << 1, -2, 3, 0.5, 0, -1;
    Eigen::MatrixXd A = u9n::EchoReservoir::Augment(S);
    CHECK(A.rows() == 4 && A.cols() == 3);
    CHECK(A.topRows(2).isApprox(S));
    CHECK(A.bottomRows(2).isApprox(S.array().square().matrix()));
}

static void CycleReservoirIsRing() {
    // The ring is orthogonal, so in the near-linear regime an input impulse keeps its shape
    // and its norm shrinks by ~SpectralRadius per step (tanh' ≈ 1 at tiny gain).
    ReservoirConfig c; c.Shape = Topology::Cycle; c.Size = 16; c.SpectralRadius = 0.9;
    c.InputScale = 1e-3; c.LeakRate = 1.0;
    EchoReservoir r(c), z(c);
    r.Step(1.0); z.Step(0.0);
    const double d0 = (r.State() - z.State()).norm();
    for (int t = 0; t < 4; ++t) { r.Step(0.0); z.Step(0.0); }
    const double ratio = (r.State() - z.State()).norm() / d0;
    CHECK(d0 > 1e-4);
    CHECK(ratio > 0.6 && ratio < 0.66);   // 0.9^4 = 0.656
}

static void ThompsonResetsOnRegimeChange() {
    GTAngelCore core(5);
    core.ChangeThreshold = 10.0;
    std::mt19937 rng(9); std::uniform_real_distribution<double> U(0, 1);
    std::array<double, 2> p{0.9, 0.1};
    Eigen::VectorXd flat = Eigen::VectorXd::Zero(GTAngelCore::ActionCount);
    for (int t = 0; t < 1000; ++t) { const int a = core.ThompsonSample(flat, 2); core.UpdateThompson(a, U(rng) < p[a] ? 1 : -1); }
    const int before = core.ChangeResets();
    CHECK(before <= 1);                 // stationary rewards: (almost) no false alarms
    std::swap(p[0], p[1]);
    int t = 0;
    for (; t < 200 && core.ChangeResets() == before; ++t) { const int a = core.ThompsonSample(flat, 2); core.UpdateThompson(a, U(rng) < p[a] ? 1 : -1); }
    CHECK(core.ChangeResets() > before);   // the swap is detected...
    CHECK(t < 60);                          // ...quickly
}

static void SelfModelPredictsHardStates() {
    // Two kinds of state: targets for states with x[0] > 0 carry noise, the rest are exact.
    // After training, the self-model must expect more error in the noisy region.
    GTAngelCore core;
    std::mt19937 rng(4); std::normal_distribution<double> N(0, 1);
    auto state = [&](double s) { Eigen::VectorXd x = 0.1 * Eigen::VectorXd::NullaryExpr(GTAngelCore::ReservoirSize, [&] { return N(rng); }); x[0] = s; return x; };
    for (int t = 0; t < 4000; ++t) {
        const double s = (t % 2) ? 0.8 : -0.8;
        Eigen::VectorXd tgt = Eigen::VectorXd::Zero(GTAngelCore::ActionCount);
        tgt[0] = s > 0 ? 0.5 * N(rng) : 0.0;
        core.TrainWout(state(s), tgt);
    }
    double hard = 0, easy = 0;
    for (int i = 0; i < 50; ++i) { hard += core.PredictErrorSq(state(0.8)); easy += core.PredictErrorSq(state(-0.8)); }
    CHECK(hard > 20 * easy);   // observed ~200x
}

int main() {
    SelfModelPredictsHardStates();
    ThompsonResetsOnRegimeChange();
    CycleReservoirIsRing();
    AugmentStacksSquares();
    KeyStoredAfterConsolidationOutsideSpan();
    RecallReinforcesSalience();
    EchoSpaceSalienceWeightedEviction();
    AgentConsolidatesEpisodicMemory();
    EchoSpaceConsolidationSeparatesSharedMode(); TrajectoryKeyShape();
    ContextualReadoutRetainsOldTask();
    ReservoirSpectralRadius(); EcanFocusesSalientCluster(); ThompsonPrefersRewardedArm(); MosesDeduplicates();
    ToroidLocksAntiPhase(); AdaptiveAttentionMatchesScheme(); EchoSpaceRecall(); NanEchoParamCount(); UnifiedCycleStreams();
    std::printf(Failures ? "%d failure(s)\n" : "all integration checks passed\n", Failures);
    return Failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
