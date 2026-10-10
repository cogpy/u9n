// agi_eval.cpp — Deep Tree Echo capability evaluation over the unified u9n stack.
//
// Each probe is a measurable task with a trivial baseline; score ∈ [0,1] is the
// fraction of the gap between baseline and ideal that the system closes.
// Capabilities that cannot be exercised in this build are reported N/A, not guessed.
//
// usage: DeepTreeEchoAGIEval [report.md] [report.json]
#include "u9n/ContextualReadout.h"
#include "u9n/EchoLanguageModel.h"
#include "u9n/UnifiedEchoAgent.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

using namespace u9n;
using Eigen::MatrixXd;
using Eigen::VectorXd;

struct Probe {
    std::string Id, Capability, Metric;
    double Value = 0, Baseline = 0, Score = -1;   // Score < 0 → N/A
    std::string Note;
};

static double Nrmse(const MatrixXd& y, const MatrixXd& t) {
    const double var = (t.array() - t.mean()).square().mean();
    return std::sqrt((y - t).array().square().mean() / var);
}
static double Clamp01(double v) { return std::clamp(v, 0.0, 1.0); }
static std::vector<VectorXd> Scalars(const std::vector<double>& u) {
    std::vector<VectorXd> v; v.reserve(u.size());
    for (double x : u) v.push_back(VectorXd::Constant(1, x));
    return v;
}

// ── 1. Working memory: Jaeger memory capacity ─────────────────────────────
// Σ_k r²(delay k) for k = 1..K; ridge readouts fit on [0, fit), scored on [fit, end).
// Σ_k r²(delay k) for k = 1..K; ridge readouts fit on [0, fit), scored on [fit, end).
// All K delay readouts share one Gram matrix, so they are fit in a single multi-target solve.
static double DelayRecall(const MatrixXd& S, const std::vector<double>& u, int wash, int fit, int end, int K, double ridge) {
    MatrixXd Y(K, S.cols());
    for (int k = 1; k <= K; ++k) for (int t = 0; t < S.cols(); ++t) Y(k - 1, t) = u[t + wash - k];
    MatrixXd W = EchoReservoir::FitRidge(S.leftCols(fit), Y.leftCols(fit), ridge);
    MatrixXd P = EchoReservoir::Predict(W, S.middleCols(fit, end - fit));
    MatrixXd Yt = Y.middleCols(fit, end - fit);
    double mc = 0;
    for (int k = 0; k < K; ++k) {
        const auto p = P.row(k).array() - P.row(k).mean(), y = Yt.row(k).array() - Yt.row(k).mean();
        const double cov = (p * y).mean();
        mc += cov * cov / (p.square().mean() * y.square().mean());
    }
    return mc;
}

static Probe MemoryCapacity() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> U(-0.5, 0.5);
    const int T = 5000, wash = 200, K = 150, train = 3500, fit = 2500;
    std::vector<double> u(T); for (auto& x : u) x = U(rng);
    const int C = T - wash;

    // Shape/regime self-selection on a validation slice of the training data (test unseen):
    // the classic sparse random ESN vs the simple cycle reservoir, each in a moderately
    // nonlinear and a near-linear regime (small input gain, ρ → 1 maximises linear memory),
    // with and without unit bias (bias pushes units off tanh's linear centre).
    struct Pick { Topology Shape; double Rho, Gain, Ridge, Bias, Val; } best{Topology::Random, 0.95, 0.1, 1e-8, 0.1, -1};
    auto make = [](Topology shape, double rho, double gain, double bias) {
        ReservoirConfig c; c.Shape = shape; c.Size = 512; c.LeakRate = 1.0; c.SpectralRadius = rho; c.InputScale = gain; c.BiasScale = bias;
        return c;
    };
    for (Topology shape : {Topology::Random, Topology::Cycle})
        for (auto [rho, gain, ridge] : {std::tuple{0.95, 0.1, 1e-8}, std::tuple{0.99, 0.01, 1e-10}})
            for (double bias : {0.1, 0.0}) {
                EchoReservoir r(make(shape, rho, gain, bias));
                const double v = DelayRecall(r.Harvest(Scalars(u), wash), u, wash, fit, train, K, ridge);
                if (v > best.Val) best = {shape, rho, gain, ridge, bias, v};
            }
    EchoReservoir r(make(best.Shape, best.Rho, best.Gain, best.Bias));
    const double mc = DelayRecall(r.Harvest(Scalars(u), wash), u, wash, train, C, K, best.Ridge);

    // K = 150 saturates once the reservoir is a clean delay line, so also report the full
    // capacity over delays 1..N (theoretical ceiling N = 512), with a longer washout.
    const int N = 512, washN = 600;
    EchoReservoir rN(make(best.Shape, best.Rho, best.Gain, best.Bias));
    const double mcN = DelayRecall(rN.Harvest(Scalars(u), washN), u, washN, train - (washN - wash), C - (washN - wash), N, best.Ridge);

    std::ostringstream note;
    note << "score = MC / K (all 150 delays recalled perfectly); selected on validation: "
         << (best.Shape == Topology::Cycle ? "cycle" : "random") << " rho=" << best.Rho << " gain=" << best.Gain
         << " bias=" << best.Bias << " (val MC " << best.Val << "); full MC over delays 1..512 = " << mcN << " of N=512";
    return {"working_memory", "Working memory (short-term trace)", "memory capacity MC (Σ r², delays 1..150)",
            mc, 0.0, Clamp01(mc / K), note.str()};
}

// ── 2. Temporal prediction: NARMA-10 ──────────────────────────────────────
static Probe Narma10() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> U(0.0, 0.5);
    const int T = 6000, wash = 200, train = 4000, fit = 3000;   // fit/validate split inside train
    std::vector<double> u(T), y(T, 0.0);
    for (auto& x : u) x = U(rng);
    for (int t = 9; t < T - 1; ++t) {
        double s = 0; for (int i = 0; i < 10; ++i) s += y[t - i];
        y[t + 1] = 0.3 * y[t] + 0.05 * y[t] * s + 1.5 * u[t - 9] * u[t] + 0.1;
    }
    MatrixXd Y(1, T - wash);
    for (int t = 0; t < Y.cols(); ++t) Y(0, t) = y[t + wash];
    const int te = int(Y.cols()) - train;

    // Timescale self-selection: the default leak (0.3) integrates over ~3 steps and smears a
    // 10-step lag window, so the reservoir picks leak / input gain / ridge on a validation
    // slice of the training data. The test segment is never seen during selection.
    struct Pick { double Leak, Gain, Ridge, Val; } best{0.3, 0.5, 1e-6, 1e300};
    for (double leak : {0.3, 0.6, 1.0})
        for (double gain : {0.05, 0.1, 0.5}) {
            ReservoirConfig c; c.LeakRate = leak; c.SpectralRadius = 0.95; c.InputScale = gain;
            EchoReservoir r(c);
            MatrixXd F = EchoReservoir::Augment(r.Harvest(Scalars(u), wash));
            for (double ridge : {1e-8, 1e-6}) {
                MatrixXd W = EchoReservoir::FitRidge(F.leftCols(fit), Y.leftCols(fit), ridge);
                const double v = Nrmse(EchoReservoir::Predict(W, F.middleCols(fit, train - fit)),
                                       Y.middleCols(fit, train - fit));
                if (v < best.Val) best = {leak, gain, ridge, v};
            }
        }
    ReservoirConfig c; c.LeakRate = best.Leak; c.SpectralRadius = 0.95; c.InputScale = best.Gain;
    EchoReservoir r(c);
    MatrixXd F = EchoReservoir::Augment(r.Harvest(Scalars(u), wash));
    MatrixXd W = EchoReservoir::FitRidge(F.leftCols(train), Y.leftCols(train), best.Ridge);
    const double esn = Nrmse(EchoReservoir::Predict(W, F.rightCols(te)), Y.rightCols(te));

    // Baseline: linear model on the last 10 raw inputs (no reservoir).
    MatrixXd L(10, Y.cols());
    for (int t = 0; t < Y.cols(); ++t) for (int i = 0; i < 10; ++i) L(i, t) = u[t + wash - i];
    MatrixXd WL = EchoReservoir::FitRidge(L.leftCols(train), Y.leftCols(train), 1e-6);
    const double lin = Nrmse(EchoReservoir::Predict(WL, L.rightCols(te)), Y.rightCols(te));
    std::ostringstream note;
    note << "baseline = linear regression on 10 lagged inputs; quadratic readout [x; x^2]; selected on validation: leak="
         << best.Leak << " gain=" << best.Gain << " ridge=" << best.Ridge << " (val NRMSE " << best.Val << ")";
    return {"temporal_prediction", "Temporal prediction (NARMA-10)", "test NRMSE", esn, lin,
            Clamp01((lin - esn) / lin), note.str()};
}

// ── 3. Nonlinear temporal reasoning: delayed parity ───────────────────────
static Probe DelayedParity() {
    std::mt19937 rng(42);
    std::bernoulli_distribution B(0.5);
    const int T = 6000, wash = 200, train = 4000, fit = 3000, maxBits = 9;
    std::vector<double> u(T); for (auto& x : u) x = B(rng) ? 1.0 : -1.0;
    const int C = T - wash;
    auto parity = [&](int bits) {
        MatrixXd Y(1, C);
        for (int t = 0; t < C; ++t) { double p = 1; for (int k = 1; k <= bits; ++k) p *= u[t + wash - k]; Y(0, t) = p; }
        return Y;
    };
    auto accuracy = [](const MatrixXd& P, const MatrixXd& Y) {
        int ok = 0; for (int t = 0; t < P.cols(); ++t) ok += (P(0, t) > 0) == (Y(0, t) > 0);
        return double(ok) / P.cols();
    };

    // Regime self-selection on a validation slice of training data (test unseen). The default
    // leak (0.3) smears consecutive ±1 symbols together, which destroys the sign products
    // parity needs. Parity 3 alone saturates, so candidates are ranked by summed validation
    // accuracy over parity 3, 5 and 7.
    struct Pick { double Leak, Gain, Rho; bool Aug; double Val; } best{0.3, 0.5, 0.9, false, -1};
    for (double leak : {0.3, 1.0})
        for (auto [gain, rho] : {std::pair{0.5, 0.9}, std::pair{1.0, 0.9}, std::pair{2.0, 0.5}}) {
            ReservoirConfig c; c.LeakRate = leak; c.InputScale = gain; c.SpectralRadius = rho;
            EchoReservoir r(c);
            const MatrixXd S = r.Harvest(Scalars(u), wash);
            for (bool aug : {false, true}) {
                const MatrixXd F = aug ? EchoReservoir::Augment(S) : S;
                double v = 0;
                for (int bits : {3, 5, 7}) {
                    const MatrixXd Y = parity(bits);
                    const MatrixXd W = EchoReservoir::FitRidge(F.leftCols(fit), Y.leftCols(fit), 1e-6);
                    v += accuracy(EchoReservoir::Predict(W, F.middleCols(fit, train - fit)), Y.middleCols(fit, train - fit));
                }
                if (v > best.Val) best = {leak, gain, rho, aug, v};
            }
        }
    ReservoirConfig c; c.LeakRate = best.Leak; c.InputScale = best.Gain; c.SpectralRadius = best.Rho;
    EchoReservoir r(c);
    MatrixXd F = r.Harvest(Scalars(u), wash);
    if (best.Aug) F = EchoReservoir::Augment(F);
    const int te = C - train;
    std::vector<double> acc(maxBits + 1, 0.0);
    int depth = 0;
    for (int bits = 1; bits <= maxBits; ++bits) {
        const MatrixXd Y = parity(bits);
        const MatrixXd W = EchoReservoir::FitRidge(F.leftCols(train), Y.leftCols(train), 1e-6);
        acc[bits] = accuracy(EchoReservoir::Predict(W, F.rightCols(te)), Y.rightCols(te));
        if (acc[bits] >= 0.95 && depth == bits - 1) depth = bits;
    }
    std::ostringstream note;
    note << "chance = 0.5; selected on validation: leak=" << best.Leak << " gain=" << best.Gain << " rho=" << best.Rho
         << (best.Aug ? " +[x;x^2]" : "") << "; test accuracy by parity order:";
    for (int bits = 3; bits <= maxBits; ++bits) note << " " << bits << ":" << acc[bits];
    note << "; deepest order solved (>=0.95 for all orders up to it): " << depth;
    return {"relational_reasoning", "Nonlinear temporal reasoning (3-bit delayed parity)", "test accuracy",
            acc[3], 0.5, Clamp01((acc[3] - 0.5) / 0.5), note.str()};
}

// ── 4. Decision making under non-stationarity: GTAngel Thompson policy ────
static Probe Bandit() {
    const int A = 10, T = 4000, runs = 20;
    double regretTS = 0, regretRnd = 0; int resets = 0;
    for (int run = 0; run < runs; ++run) {
        std::mt19937 rng(100 + run);
        std::uniform_real_distribution<double> U(0, 1);
        std::vector<double> p(A); for (auto& x : p) x = U(rng) * 0.8 + 0.1;
        GTAngelCore core(run);
        core.ChangeThreshold = 10.0;
        VectorXd flat = VectorXd::Zero(GTAngelCore::ActionCount);
        for (int t = 0; t < T; ++t) {
            if (t == T / 2) { std::shuffle(p.begin(), p.end(), rng); }   // regime change
            const double best = *std::max_element(p.begin(), p.end());
            const int a = core.ThompsonSample(flat, A);
            const double r = U(rng) < p[a] ? 1.0 : -1.0;
            core.UpdateThompson(a, r);
            regretTS  += best - p[a];
            regretRnd += best - p[std::uniform_int_distribution<int>(0, A - 1)(rng)];
        }
        resets += core.ChangeResets();
    }
    return {"adaptive_decision", "Adaptive decision-making (non-stationary 10-arm bandit)", "cumulative regret / run",
            regretTS / runs, regretRnd / runs, Clamp01(1.0 - regretTS / regretRnd),
            "GTAngel Thompson sampler + Page-Hinkley change-point reset (threshold 10, drift 0.05; "
            + std::to_string(resets) + " resets over " + std::to_string(runs) + " runs, 1 true change each); "
            "baseline = uniform random; arms reshuffled mid-run"};
}

// ── 5. Selective attention: ECAN tracks the salient cluster ───────────────
static Probe Attention() {
    std::mt19937 rng(7);
    std::normal_distribution<double> N(0, 0.3);
    int hits = 0, trials = 0; double latency = 0;
    GTAngelCore core;
    int prev = -1;
    for (int trial = 0; trial < 32; ++trial) {
        const int target = std::uniform_int_distribution<int>(0, GTAngelCore::ClusterCount - 1)(rng);
        int switched = -1;
        for (int t = 0; t < 60; ++t) {
            VectorXd x = VectorXd::NullaryExpr(GTAngelCore::ReservoirSize, [&] { return N(rng); });
            x.segment(target * GTAngelCore::ClusterSize, GTAngelCore::ClusterSize).array() *= 2.5;
            core.UpdateAttention(x);
            if (switched < 0 && core.TopCluster() == target) switched = t;
        }
        if (target != prev) { latency += switched < 0 ? 60 : switched; }
        prev = target;
        hits += core.TopCluster() == target; ++trials;
    }
    const double acc = double(hits) / trials;
    std::ostringstream n; n << "chance = 1/16; mean refocus latency " << latency / trials << " steps";
    return {"selective_attention", "Selective attention (ECAN STI refocusing)", "top-cluster accuracy",
            acc, 1.0 / 16, Clamp01((acc - 1.0 / 16) / (1 - 1.0 / 16)), n.str()};
}

// ── 6. Episodic memory: EchoSpace recall of reservoir-encoded sequences ───
// Baseline: final-state key + raw cosine (the original EchoSpace path).
// System:   trajectory key + consolidated (whitened) EchoSpace.
// Scored at two cue-noise levels so the probe does not saturate.
static Probe EpisodicMemory() {
    EchoReservoir r;
    std::normal_distribution<double> N(0, 1);
    const int items = 200, len = 20;
    std::vector<std::vector<double>> seqs(items, std::vector<double>(len));
    for (auto& s : seqs) for (auto& x : s) x = 0.5 * N(r.Random());

    auto finalState = [&](const std::vector<double>& s) { r.Reset(); for (double x : s) r.Step(x); return VectorXd(r.State()); };
    EchoSpace naive;
    for (int i = 0; i < items; ++i) naive.Store(finalState(seqs[i]), i);

    // Encoder self-selection by rehearsal: the memory replays its own stored episodes under
    // simulated corruption (its own RNG stream, σ=0.35) and keeps the encoder that recalls
    // them best. The test cues are never seen. Candidates: the default leaky random ESN with
    // whitening, and cycle reservoirs (orthogonal ring ⇒ near-isometric encoding of the input
    // history) with raw cosine or whitening.
    struct Enc { const char* Name; ReservoirConfig Cfg; bool Whiten; };
    auto cyc = [](double gain, double rho) { ReservoirConfig c; c.Shape = Topology::Cycle; c.LeakRate = 1.0; c.InputScale = gain; c.SpectralRadius = rho; return c; };
    const std::vector<Enc> encs = {{"random+whiten", ReservoirConfig{}, true},
                                   {"cycle(0.1,0.9)", cyc(0.1, 0.9), false}, {"cycle(0.1,0.9)+whiten", cyc(0.1, 0.9), true},
                                   {"cycle(0.1,0.8)", cyc(0.1, 0.8), false}, {"cycle(0.05,0.95)", cyc(0.05, 0.95), false}};
    std::mt19937 rehearsal(777);
    int bestEnc = 0, bestHits = -1;
    for (int e = 0; e < (int)encs.size(); ++e) {
        EchoReservoir enc(encs[e].Cfg); EchoSpace m;
        for (int i = 0; i < items; ++i) m.Store(enc.EncodeTrajectory(seqs[i]), i);
        if (encs[e].Whiten) m.Consolidate();
        int hits = 0;
        for (int i = 0; i < items; ++i) {
            auto q = seqs[i]; for (auto& x : q) x += 0.35 * N(rehearsal);
            hits += m.Recall(enc.EncodeTrajectory(q)) == i;
        }
        if (hits > bestHits) { bestHits = hits; bestEnc = e; }
    }
    EchoReservoir enc(encs[bestEnc].Cfg); EchoSpace mem;
    for (int i = 0; i < items; ++i) mem.Store(enc.EncodeTrajectory(seqs[i]), i);
    if (encs[bestEnc].Whiten) mem.Consolidate();

    double accSum = 0, naiveSum = 0, rawSum = 0; std::ostringstream n;
    n << "200 stored sequences; recall (final-state+raw cosine -> selected encoder; raw-input cosine ceiling):";
    for (double sigma : {0.25, 0.5}) {
        int okNaive = 0, ok = 0, okRaw = 0;
        for (int i = 0; i < items; ++i) {
            auto q = seqs[i];
            for (auto& x : q) x += sigma * N(r.Random());
            okNaive += naive.Recall(finalState(q)) == i;
            ok      += mem.Recall(enc.EncodeTrajectory(q)) == i;
            // Ceiling reference: nearest stored raw sequence by cosine (no reservoir at all).
            const VectorXd qv = Eigen::Map<const VectorXd>(q.data(), len);
            int best = -1; double bs = -2;
            for (int j = 0; j < items; ++j) {
                const double c = qv.dot(Eigen::Map<const VectorXd>(seqs[j].data(), len)) / (qv.norm() * Eigen::Map<const VectorXd>(seqs[j].data(), len).norm());
                if (c > bs) { bs = c; best = j; }
            }
            okRaw += best == i;
        }
        accSum += double(ok) / items; naiveSum += double(okNaive) / items; rawSum += double(okRaw) / items;
        n << " sigma=" << sigma << ": " << double(okNaive) / items << " -> " << double(ok) / items << " (raw " << double(okRaw) / items << ");";
    }
    const double acc = accSum / 2, accNaive = naiveSum / 2;
    n << " encoder chosen by rehearsal: " << encs[bestEnc].Name << " (" << bestHits << "/200 at sigma=0.35)";
    return {"episodic_memory", "Episodic memory (noisy cue → sequence recall, EchoSpace)", "mean recall (σ=0.25, 0.5)",
            acc, accNaive, Clamp01((acc - accNaive) / (1.0 - accNaive)), n.str()};
}

// ── 7. Continual learning: sequential tasks A → B → C without task labels ───
// Single online Wout (GTAngel) vs ContextualReadout (context-routed head bank).
static Probe ContinualLearning() {
    const double freqs[3] = {0.2, 0.05, 0.11};
    EchoReservoir r;
    GTAngelCore single; single.LearningRate = 2e-3;
    ContextualReadout bank(r.Config().Size, 1);

    auto sgd = [&](double f, int steps) {
        r.Reset(); bank.ResetContext();
        for (int t = 0; t < steps; ++t) {
            r.Step(std::sin(f * t));
            bank.Observe(r.State());
            if (t <= 50) continue;
            VectorXd tgt = VectorXd::Zero(GTAngelCore::ActionCount); tgt[0] = std::sin(f * (t + 3));
            single.TrainWout(r.State(), tgt);
            bank.Train(r.State(), tgt.head(1));
        }
    };
    // Returns {single NRMSE, bank NRMSE}; bank is scored only after its context settles.
    auto test = [&](double f) {
        r.Reset(); bank.ResetContext();
        double se1 = 0, se2 = 0, sv = 0;
        for (int t = 0; t < 800; ++t) {
            r.Step(std::sin(f * t));
            bank.Observe(r.State());
            if (t <= 200) continue;
            const double y = std::sin(f * (t + 3));
            se1 += std::pow(single.Readout().row(0).dot(r.State()) - y, 2);
            se2 += std::pow(bank.Predict(r.State())[0] - y, 2);
            sv += y * y;
        }
        return std::make_pair(std::sqrt(se1 / sv), std::sqrt(se2 / sv));
    };
    for (double f : freqs) sgd(f, 6000);
    double worst1 = 0, worst2 = 0; std::ostringstream n;
    n << "after A->B->C, per-task NRMSE single/bank:";
    for (double f : freqs) {
        auto [e1, e2] = test(f);
        worst1 = std::max(worst1, e1); worst2 = std::max(worst2, e2);
        n << " " << f << ":" << e1 << "/" << e2;
    }
    n << "; heads spawned " << bank.HeadCount() << "; baseline = single GTAngel Wout (worst-task NRMSE)";
    return {"continual_learning", "Continual learning (3 sequential tasks, no task labels)", "worst-task NRMSE after all tasks",
            worst2, worst1, Clamp01(1.0 - worst2), n.str()};
}

// ── 8. Metacognition: does the agent know how wrong it is about to be? ─────
static double Pearson(const std::vector<double>& a, const std::vector<double>& b) {
    const int m = (int)a.size();
    double ma = 0, mb = 0; for (int i = 0; i < m; ++i) { ma += a[i]; mb += b[i]; } ma /= m; mb /= m;
    double cv = 0, va = 0, vb = 0;
    for (int i = 0; i < m; ++i) { cv += (a[i] - ma) * (b[i] - mb); va += std::pow(a[i] - ma, 2); vb += std::pow(b[i] - mb, 2); }
    return cv / std::sqrt(va * vb + 1e-300);
}

static Probe Metacognition() {
    EchoReservoir r;
    GTAngelCore core;
    auto sig = [](int t) { return std::sin(0.13 * t) * std::cos(0.031 * t); };
    std::vector<double> coh, err, self;
    double se = 0, sv = 0, sp = 0;
    for (int t = 0; t < 10000; ++t) {
        r.Step(sig(t));
        const double y = sig(t + 2);
        // Self-prediction and true error are both taken *before* training on this sample.
        const double p = core.Readout().row(0).dot(r.State());
        sp += core.PredictErrorSq(r.State()) * GTAngelCore::ActionCount;   // per-action mean → row-0 error
        se += (p - y) * (p - y); sv += y * y;
        VectorXd tgt = VectorXd::Zero(GTAngelCore::ActionCount); tgt[0] = y;
        core.TrainWout(r.State(), tgt);
        core.UpdateAttention(r.State());
        core.MinePatterns(r.State(), 1.0 - 2.0 * std::min(1.0, std::abs(p - y)));
        if (t % 250 == 249) {
            coh.push_back(core.ComputeCoherence().Overall); err.push_back(std::sqrt(se / sv)); self.push_back(std::sqrt(sp / sv));
            se = sv = sp = 0;
        }
    }
    const double rSelf = Pearson(self, err), rCoh = Pearson(coh, err);
    std::ostringstream note;
    note << "prospective self-model (error predicted from state before the target is seen); "
            "retrospective coherence r = " << rCoh << " for comparison";
    return {"metacognition", "Metacognitive self-monitoring (predicted vs true error)", "Pearson r(self-predicted NRMSE, held-out NRMSE)",
            rSelf, 0.0, Clamp01(rSelf), note.str()};
}

// ── 9. Hemispheric integration: EchoSelf toroid phase lock ────────────────
static Probe Toroidal() {
    Toroid t;
    for (int i = 0; i < 2000; ++i) t.Advance(0.05);
    double c = 0; int insights = 0;
    for (int i = 0; i < 2000; ++i) { t.Advance(0.05); c += t.Coherence(); insights += t.InsightWindow(); }
    c /= 2000;
    std::ostringstream n; n << insights << " insight windows / 2000 steps; detuning Δω=0.07, K=0.4";
    return {"hemispheric_integration", "Dual-hemisphere integration (DTE ⟷ Marduk toroid)", "mean anti-phase coherence",
            c, 0.5, Clamp01((c - 0.5) / 0.5), n.str()};
}

// ── 10. Closed-loop embodiment: unified agent in a contextual bandit ──────
static Probe Embodied() {
    const int A = 4, T = 12000;
    UnifiedEchoAgent agent(1, 11);
    std::mt19937 rng(3);
    std::uniform_real_distribution<double> U(0, 1);
    double reward = 0, got = 0; int acts = 0;
    for (int t = 0; t < T; ++t) {
        const int ctx = (t / 60) % A;                   // slowly changing context signal
        VectorXd in = VectorXd::Constant(1, (ctx - 1.5) / 1.5);
        const int a = agent.Tick(in, reward, A);
        if (a >= 0) {
            reward = (a == ctx ? (U(rng) < 0.9) : (U(rng) < 0.1)) ? 1.0 : -1.0;
            if (t > T / 2) { got += reward > 0; ++acts; }
            // supervised hint through Wout (GTAngel imitation channel) on the correct action
            VectorXd tgt = VectorXd::Zero(GTAngelCore::ActionCount); tgt[ctx] = 1.0;
            agent.Core.TrainWout(agent.Reservoir.State(), tgt);
        }
    }
    const double rate = got / acts, chance = 0.9 / A + 0.1 * (A - 1) / A;
    std::ostringstream n; n << "EchoSpace holds " << agent.Memory.Size() << " episodes (" << agent.Consolidations() << " consolidations); "
                            << agent.Core.MinedPatterns().size() << " MOSES patterns";
    return {"closed_loop_agency", "Closed-loop contextual agency (12-step unified agent)", "reward rate (2nd half)",
            rate, chance, Clamp01((rate - chance) / (0.9 - chance)), n.str()};
}

// ── 11. Language / open-ended reasoning: NanEcho 48M ──────────────────────
// ── 11. Language modelling: character-level EchoLanguageModel on the EchoSelf corpus ──
// The NanEcho 48M transformer has no reachable checkpoint (the 9cog/echoself-nanecho Hub repo
// is not public) and no torch runtime here, so u9n's own language model is measured instead:
// the reservoir echo readout mixed with episodic n-gram counts. Text: the four Deep Tree Echo
// self-knowledge sets from 9cog/EchoSelf-48M data/ (introspection, identity, autognosis,
// somatic), split by question/answer pair round-robin (8 train : 1 validation : 1 test).
#ifndef U9N_EVAL_DATA_DIR
#define U9N_EVAL_DATA_DIR "eval/data"
#endif
static Probe Language() {
    std::ifstream f(std::string(U9N_EVAL_DATA_DIR) + "/echoself_corpus.txt");
    if (!f) return {"language_reasoning", "Language modelling (char-level, EchoSelf corpus)", "bits per character",
                    0, 0, -1, "corpus file eval/data/echoself_corpus.txt not found → not evaluated"};
    std::stringstream ss; ss << f.rdbuf(); const std::string text = ss.str();
    std::vector<std::string> lines; { std::stringstream ls(text); std::string l; while (std::getline(ls, l)) lines.push_back(l); }
    std::string part[3];
    for (size_t i = 0; i < lines.size(); ++i) { const int k = int(i / 2) % 10; part[k < 8 ? 0 : (k == 8 ? 1 : 2)] += lines[i] + "\n"; }

    EchoLanguageModel lm;
    lm.Fit(text, part[0]);
    lm.Calibrate(part[1]);
    double echoOnly = 0, ngramOnly = 0;
    const double bpc = lm.BitsPerChar(part[2], &echoOnly, &ngramOnly);

    // Baseline: unigram character frequencies of the training text (add-one), on the test text.
    std::map<char, double> uni; for (char c : text) uni[c] = 1.0;
    for (char c : part[0]) uni[c] += 1.0;
    double tot = 0; for (auto& kv : uni) tot += kv.second;
    double hUni = 0; for (size_t t = 1; t < part[2].size(); ++t) hUni -= std::log2(uni[part[2][t]] / tot);
    hUni /= double(part[2].size() - 1);

    std::mt19937 rng(7);
    std::string sample = lm.Generate("What is ", 120, rng);
    for (auto& c : sample) if (c == '\n' || c == '|') c = ' ';
    NanEchoSpec s;
    std::ostringstream n;
    n << "u9n EchoLanguageModel (512-unit reservoir, quadratic readout, mixed with Witten-Bell order-12 n-gram counts "
         "that keep learning while reading; echo weight per matched context depth 0..12:";
    for (double w : lm.MixtureWeights()) n << " " << w;
    n << "); test bpc: mixture " << bpc << ", echo only " << echoOnly << ", n-gram only " << ngramOnly
      << ", unigram baseline " << hUni << "; score = 1 - bpc/unigram; sample after 'What is ': \"" << sample
      << "\"; NanEcho " << s.Params() / 1e6 << "M itself not evaluated (no public checkpoint); "
      << "this measures next-character prediction, not open-ended reasoning";
    return {"language_reasoning", "Language modelling (char-level, EchoSelf corpus)", "test bits per character",
            bpc, hUni, Clamp01(1.0 - bpc / hUni), n.str()};
}

int main(int argc, char** argv) {
    const std::string md = argc > 1 ? argv[1] : "AGI_EVALUATION_RESULTS.md";
    const std::string js = argc > 2 ? argv[2] : "agi_evaluation_results.json";
    std::vector<Probe (*)()> fns = {MemoryCapacity, Narma10, DelayedParity, Bandit, Attention,
                                    EpisodicMemory, ContinualLearning, Metacognition, Toroidal, Embodied, Language};
    std::vector<Probe> ps;
    for (auto f : fns) { ps.push_back(f()); std::cerr << "✓ " << ps.back().Id << "\n"; }

    double sum = 0; int k = 0;
    for (auto& p : ps) if (p.Score >= 0) { sum += p.Score; ++k; }
    const double mean = sum / k, coverage = double(k) / ps.size();

    std::ofstream o(md);
    o << "# Deep Tree Echo — Capability Evaluation (generated)\n\n"
      << "Generated by `Integration/eval/agi_eval.cpp`. Score = fraction of the baseline→ideal gap closed.\n\n"
      << "| Capability | Metric | Value | Baseline | Score |\n|---|---|---:|---:|---:|\n";
    char buf[64];
    for (auto& p : ps) {
        o << "| " << p.Capability << " | " << p.Metric << " | ";
        std::snprintf(buf, sizeof buf, "%.4g | %.4g | ", p.Value, p.Baseline); o << buf;
        if (p.Score < 0) o << "N/A |\n"; else { std::snprintf(buf, sizeof buf, "%.2f |\n", p.Score); o << buf; }
    }
    std::snprintf(buf, sizeof buf, "%.2f", mean);
    o << "\n**Mean score over evaluated probes:** " << buf;
    std::snprintf(buf, sizeof buf, "%.0f%%", coverage * 100);
    o << " — **coverage:** " << buf << " of listed capabilities\n\n### Notes\n\n";
    for (auto& p : ps) o << "- **" << p.Id << "**: " << p.Note << "\n";

    std::ofstream j(js);
    j << "{\n  \"mean_score\": " << mean << ",\n  \"coverage\": " << coverage << ",\n  \"probes\": [\n";
    for (size_t i = 0; i < ps.size(); ++i) {
        auto& p = ps[i];
        j << "    {\"id\": \"" << p.Id << "\", \"value\": " << p.Value << ", \"baseline\": " << p.Baseline
          << ", \"score\": " << (p.Score < 0 ? std::string("null") : std::to_string(p.Score)) << "}"
          << (i + 1 < ps.size() ? "," : "") << "\n";
    }
    j << "  ]\n}\n";
    std::cout << std::ifstream(md).rdbuf();
    return 0;
}
