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

int main() {
    ContextualReadoutRetainsOldTask();
    ReservoirSpectralRadius(); EcanFocusesSalientCluster(); ThompsonPrefersRewardedArm(); MosesDeduplicates();
    ToroidLocksAntiPhase(); AdaptiveAttentionMatchesScheme(); EchoSpaceRecall(); NanEchoParamCount(); UnifiedCycleStreams();
    std::printf(Failures ? "%d failure(s)\n" : "all integration checks passed\n", Failures);
    return Failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
