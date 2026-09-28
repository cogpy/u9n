// agi_eval.cpp — Deep Tree Echo capability evaluation over the unified u9n stack.
//
// Each probe is a measurable task with a trivial baseline; score ∈ [0,1] is the
// fraction of the gap between baseline and ideal that the system closes.
// Capabilities that cannot be exercised in this build are reported N/A, not guessed.
//
// usage: DeepTreeEchoAGIEval [report.md] [report.json]
#include "u9n/ContextualReadout.h"
#include "u9n/UnifiedEchoAgent.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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
static Probe MemoryCapacity() {
    ReservoirConfig c; c.Size = 512; c.InputScale = 0.1; c.LeakRate = 1.0; c.SpectralRadius = 0.95;
    EchoReservoir r(c);
    std::uniform_real_distribution<double> U(-0.5, 0.5);
    const int T = 5000, wash = 200, K = 150, train = 3500;
    std::vector<double> u(T); for (auto& x : u) x = U(r.Random());
    MatrixXd S = r.Harvest(Scalars(u), wash);
    double mc = 0;
    for (int k = 1; k <= K; ++k) {
        MatrixXd Y(1, S.cols());
        for (int t = 0; t < S.cols(); ++t) Y(0, t) = u[t + wash - k];
        MatrixXd W = EchoReservoir::FitRidge(S.leftCols(train), Y.leftCols(train), 1e-8);
        MatrixXd P = EchoReservoir::Predict(W, S.rightCols(S.cols() - train));
        MatrixXd Yt = Y.rightCols(S.cols() - train);
        const double cov = ((P.array() - P.mean()) * (Yt.array() - Yt.mean())).mean();
        const double r2 = cov * cov / ((P.array() - P.mean()).square().mean() * (Yt.array() - Yt.mean()).square().mean());
        mc += r2;
    }
    // Theoretical ceiling for linear-ish reservoirs is N; practical tanh ESNs reach ~N/10..N/5.
    return {"working_memory", "Working memory (short-term trace)", "memory capacity MC (Σ r², delays 1..150)",
            mc, 0.0, Clamp01(mc / 100.0), "normalised against 100 delays recalled perfectly; ceiling N=512"};
}

// ── 2. Temporal prediction: NARMA-10 ──────────────────────────────────────
static Probe Narma10() {
    EchoReservoir r;
    std::uniform_real_distribution<double> U(0.0, 0.5);
    const int T = 6000, wash = 200, train = 4000;
    std::vector<double> u(T), y(T, 0.0);
    for (auto& x : u) x = U(r.Random());
    for (int t = 9; t < T - 1; ++t) {
        double s = 0; for (int i = 0; i < 10; ++i) s += y[t - i];
        y[t + 1] = 0.3 * y[t] + 0.05 * y[t] * s + 1.5 * u[t - 9] * u[t] + 0.1;
    }
    MatrixXd S = r.Harvest(Scalars(u), wash);
    MatrixXd Y(1, S.cols());
    for (int t = 0; t < S.cols(); ++t) Y(0, t) = y[t + wash];
    const int te = int(S.cols()) - train;
    MatrixXd W = EchoReservoir::FitRidge(S.leftCols(train), Y.leftCols(train), 1e-6);
    const double esn = Nrmse(EchoReservoir::Predict(W, S.rightCols(te)), Y.rightCols(te));

    // Baseline: linear model on the last 10 raw inputs (no reservoir).
    MatrixXd L(10, S.cols());
    for (int t = 0; t < S.cols(); ++t) for (int i = 0; i < 10; ++i) L(i, t) = u[t + wash - i];
    MatrixXd WL = EchoReservoir::FitRidge(L.leftCols(train), Y.leftCols(train), 1e-6);
    const double lin = Nrmse(EchoReservoir::Predict(WL, L.rightCols(te)), Y.rightCols(te));
    return {"temporal_prediction", "Temporal prediction (NARMA-10)", "test NRMSE", esn, lin,
            Clamp01((lin - esn) / lin), "baseline = linear regression on 10 lagged inputs"};
}

// ── 3. Nonlinear temporal reasoning: delayed parity ───────────────────────
static Probe DelayedParity() {
    EchoReservoir r;
    std::bernoulli_distribution B(0.5);
    const int T = 6000, wash = 200, train = 4000;
    std::vector<double> u(T); for (auto& x : u) x = B(r.Random()) ? 1.0 : -1.0;
    MatrixXd S = r.Harvest(Scalars(u), wash);
    MatrixXd Y(1, S.cols());
    for (int t = 0; t < S.cols(); ++t) Y(0, t) = u[t + wash - 1] * u[t + wash - 2] * u[t + wash - 3];
    const int te = int(S.cols()) - train;
    MatrixXd P = EchoReservoir::Predict(EchoReservoir::FitRidge(S.leftCols(train), Y.leftCols(train), 1e-6), S.rightCols(te));
    int ok = 0; for (int t = 0; t < te; ++t) ok += (P(0, t) > 0) == (Y(0, train + t) > 0);
    const double acc = double(ok) / te;
    return {"relational_reasoning", "Nonlinear temporal reasoning (3-bit delayed parity)", "test accuracy",
            acc, 0.5, Clamp01((acc - 0.5) / 0.5), "chance = 0.5; linear readouts cannot solve parity without a nonlinear reservoir"};
}

// ── 4. Decision making under non-stationarity: GTAngel Thompson policy ────
static Probe Bandit() {
    const int A = 10, T = 4000, runs = 20;
    double regretTS = 0, regretRnd = 0;
    for (int run = 0; run < runs; ++run) {
        std::mt19937 rng(100 + run);
        std::uniform_real_distribution<double> U(0, 1);
        std::vector<double> p(A); for (auto& x : p) x = U(rng) * 0.8 + 0.1;
        GTAngelCore core(run);
        VectorXd flat = VectorXd::Zero(GTAngelCore::ActionCount);
        for (int t = 0; t < T; ++t) {
            if (t == T / 2) { std::shuffle(p.begin(), p.end(), rng); }   // regime change
            const double best = *std::max_element(p.begin(), p.end());
            const int a = core.ThompsonSample(flat, A);
            const double r = U(rng) < p[a] ? 1.0 : -1.0;
            core.UpdateThompson(a, r);
            core.DiscountThompson(0.995);
            regretTS  += best - p[a];
            regretRnd += best - p[std::uniform_int_distribution<int>(0, A - 1)(rng)];
        }
    }
    return {"adaptive_decision", "Adaptive decision-making (non-stationary 10-arm bandit)", "cumulative regret / run",
            regretTS / runs, regretRnd / runs, Clamp01(1.0 - regretTS / regretRnd),
            "GTAngel Thompson sampler + discounting; baseline = uniform random; arms reshuffled mid-run"};
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
static Probe EpisodicMemory() {
    EchoReservoir r;
    std::normal_distribution<double> N(0, 1);
    const int items = 200, len = 20;
    std::vector<std::vector<double>> seqs(items, std::vector<double>(len));
    for (auto& s : seqs) for (auto& x : s) x = 0.5 * N(r.Random());
    EchoSpace mem;
    auto encode = [&](const std::vector<double>& s) { r.Reset(); for (double x : s) r.Step(x); return VectorXd(r.State()); };
    for (int i = 0; i < items; ++i) mem.Store(encode(seqs[i]), i, EchoSpace::Kind::Episodic);
    int ok = 0;
    for (int i = 0; i < items; ++i) {
        auto q = seqs[i];
        for (auto& x : q) x += 0.25 * N(r.Random());      // corrupted cue (SNR ≈ 4)
        ok += mem.Recall(encode(q)) == i;
    }
    const double acc = double(ok) / items;
    return {"episodic_memory", "Episodic memory (noisy cue → sequence recall, EchoSpace)", "recall accuracy",
            acc, 1.0 / items, Clamp01((acc - 1.0 / items) / (1 - 1.0 / items)), "200 stored sequences, cue noise σ=0.25"};
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

// ── 8. Metacognition: does GTAngel coherence track real competence? ───────
static Probe Metacognition() {
    EchoReservoir r;
    GTAngelCore core;
    auto sig = [](int t) { return std::sin(0.13 * t) * std::cos(0.031 * t); };
    std::vector<double> coh, err;
    double se = 0, sv = 0;
    for (int t = 0; t < 10000; ++t) {
        r.Step(sig(t));
        const double y = sig(t + 2);
        // Measure the prediction error *before* training on this sample (true held-out error).
        const double p = core.Readout().row(0).dot(r.State());
        se += (p - y) * (p - y); sv += y * y;
        VectorXd tgt = VectorXd::Zero(GTAngelCore::ActionCount); tgt[0] = y;
        core.TrainWout(r.State(), tgt);
        core.UpdateAttention(r.State());
        core.MinePatterns(r.State(), 1.0 - 2.0 * std::min(1.0, std::abs(p - y)));
        if (t % 250 == 249) { coh.push_back(core.ComputeCoherence().Overall); err.push_back(std::sqrt(se / sv)); se = sv = 0; }
    }
    const int m = (int)coh.size();
    double mc = 0, me = 0; for (int i = 0; i < m; ++i) { mc += coh[i]; me += err[i]; } mc /= m; me /= m;
    double cv = 0, vc = 0, ve = 0;
    for (int i = 0; i < m; ++i) { cv += (coh[i] - mc) * (err[i] - me); vc += std::pow(coh[i] - mc, 2); ve += std::pow(err[i] - me, 2); }
    const double corr = cv / std::sqrt(vc * ve + 1e-300);
    return {"metacognition", "Metacognitive self-monitoring (coherence vs true error)", "Pearson r(coherence, held-out NRMSE)",
            corr, 0.0, Clamp01(-corr),
            "calibrated self-model => strongly negative r; coherence shares the loss EMA, so this upper-bounds introspection"};
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
    std::ostringstream n; n << "EchoSpace consolidated " << agent.Memory.Size() << " episodes; "
                            << agent.Core.MinedPatterns().size() << " MOSES patterns";
    return {"closed_loop_agency", "Closed-loop contextual agency (12-step unified agent)", "reward rate (2nd half)",
            rate, chance, Clamp01((rate - chance) / (0.9 - chance)), n.str()};
}

// ── 11. Language / open-ended reasoning: NanEcho 48M ──────────────────────
static Probe Language() {
    NanEchoSpec s;
    std::ostringstream n;
    n << "architecture " << s.Layers << "L/" << s.Heads << "H/" << s.Embed << "d ≈ " << s.Params() / 1e6
      << "M params; no trained checkpoint or torch runtime in this build → not evaluated";
    return {"language_reasoning", "Language & open-ended reasoning (NanEcho 48M)", "perplexity / benchmark",
            0, 0, -1, n.str()};
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
