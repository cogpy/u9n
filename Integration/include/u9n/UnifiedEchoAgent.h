// UnifiedEchoAgent.h — u9n × GTAngelEcho × EchoSelf-48M in one 12-step cognitive cycle.
//
//   step k ∈ 1..12, stream s = (k-1) mod 3
//     s=0 Perceiving  : reservoir ← input; ECAN attention (GTAngel)
//     s=1 Acting      : attention-gated logits → Thompson action (GTAngel)
//     s=2 Reflecting  : MOSES mining, toroid advance, resonance log (EchoSelf)
//   step 12 (cycle end): the cycle's four perceived states form one episode key (trajectory
//   encoding); it is stored in EchoSpace when the hemispheres are coherent, with salience
//   coherence × cycle |reward| driving eviction, and every
//   ConsolidateEvery stored episodes EchoSpace re-fits its whitening ("sleep" consolidation).
//
// Integration (u9n): the strengthened sub-systems live inside the agent, not only in probes.
//   memory channel — a bias-free cycle reservoir (orthogonal delay line) stepped with the main
//                    reservoir on Perceiving steps, so cues survive delays;
//   cycle frame    — a small second ring, cleared at each cycle start and read un-rotated by
//                    the cycle phase: each Perceiving slot of this cycle keeps fixed coordinates
//                    at every Acting step, so the value readout sees "the cue of this cycle"
//                    in one place whether it arrived ten steps ago or is still present;
//   value learning — the agent learns from its own reward: Q = Wq·[echo; frame; main; 1] by
//                    recursive least squares on the action taken; actions are sampled from
//                    softmax((gated logits + Q) / T);
//   change points  — Page-Hinkley on the agent's own value shortfall; an alarm clears Wq
//                    (the state covariance P is kept: the inputs did not change, the rule did);
//   self-model     — per action, a readout predicts the log squared value error before reward
//                    arrives; Confidence() turns it and Q into the probability the action taken
//                    is rewarded, and low certainty in the best option raises the exploration
//                    temperature;
//   voice          — an optional EchoLanguageModel the agent can speak through (Say()).
#pragma once

#include <algorithm>
#include <array>

#include "EchoLanguageModel.h"
#include "EchoReservoir.h"
#include "EchoSelfCore.h"
#include "GTAngelCore.h"

namespace u9n {

class UnifiedEchoAgent {
public:
    enum class Stream { Perceiving = 0, Acting = 1, Reflecting = 2 };

    // Consolidation costs O(M²·D + M³) for M stored episodes of dimension D, so memory is
    // bounded and consolidation is amortised over many cycles rather than run every store.
    int ConsolidateEvery = 128;

    // Integration switches and rates (see header comment).
    bool Integrated = true;
    static constexpr int EchoSize = 256, LocalSize = 16;   // LocalSize ≥ 4 slots per cycle
    double ValueForget = 0.999;          // RLS forgetting factor for the value readout
    bool Boltzmann = true;               // integrated: sample softmax((logits + Q)/T), not Thompson
    double TempMin = 0.01, TempMax = 0.3; // exploration temperature range (confident → unsure)
    // Change detection on the agent's own contextual prediction: per-action Page-Hinkley on
    // the shortfall Q_a − r. (GTAngel's detector compares reward with a context-free Beta
    // mean, which fires on every context switch inside a contextual task.)
    double ChangeThreshold = 40.0, ChangeDrift = 0.8;

    explicit UnifiedEchoAgent(int inputDim = 1, unsigned seed = 42, size_t memoryCapacity = 512)
        : Reservoir(MakeCfg(inputDim, seed)), Echo(MakeEchoCfg(inputDim, seed)), Local(MakeLocalCfg(inputDim, seed)), Core(seed + 1), Rng(seed + 13) {
        Memory.Capacity = memoryCapacity;
        Wq = Eigen::MatrixXd::Zero(GTAngelCore::ActionCount, Features());
        Wm = Eigen::MatrixXd::Zero(GTAngelCore::ActionCount, Features());
        P = Eigen::MatrixXd::Identity(Features(), Features()) * 100.0;
    }

    // One cycle step. Returns chosen action on Acting steps, -1 otherwise.
    int Tick(const Eigen::VectorXd& input, double reward, int nActions = GTAngelCore::ActionCount) {
        Step = Step % 12 + 1;
        const Stream s = Stream((Step - 1) % 3);
        int action = -1;
        switch (s) {
        case Stream::Perceiving:
            Reservoir.Step(input);
            if (Integrated) {
                Echo.Step(input);
                if (Step == 1) Local.Reset();   // a fresh frame each cycle
                Local.Step(input);
            }
            Core.UpdateAttention(Reservoir.State());
            CycleTrace.segment(((Step - 1) / 3) * GTAngelCore::ReservoirSize, GTAngelCore::ReservoirSize) = Reservoir.State();
            break;
        case Stream::Acting: {
            if (LastAction >= 0) {
                Core.UpdateThompson(LastAction, reward);
                if (Integrated) LearnValue(reward);
            }
            Eigen::VectorXd logits = Core.GatedLogits(Reservoir.State());
            if (Integrated) {
                LastF = AgentFeatures();
                LastQ = Wq * LastF;
                // explore in proportion to how unsure the agent is about its best option
                int best = 0; LastQ.head(nActions).maxCoeff(&best);
                const double conf = 1.0 / (1.0 + std::sqrt(std::exp(Wm.row(best).dot(LastF))));
                const double temp = TempMin + (TempMax - TempMin) * (1.0 - conf);
                logits = (logits + LastQ) / temp;
            }
            action = LastAction = (Integrated && Boltzmann) ? SampleSoftmax(logits, nActions)
                                                             : Core.ThompsonSample(logits, nActions);
            if (Integrated) LastErr = std::exp(Wm.row(action).dot(LastF));   // about the action taken
            break;
        }
        case Stream::Reflecting:
            Core.MinePatterns(Reservoir.State(), reward);
            Torus.Advance(0.1);
            Resonance.Record(reward);
            break;
        }
        CycleRewardAbs += std::abs(reward);
        if (Step == 12) EndCycle();
        return action;
    }

    // Nearest stored episode (its cycle index) for a key of the same form as LastEpisodeKey().
    // By default a confident recall reinforces the episode (salience boost + rehearsal), so
    // episodes the agent keeps using survive eviction; pass reinforce=false for a pure lookup.
    int RecallEpisode(const Eigen::VectorXd& key, double* sim = nullptr, bool reinforce = true) {
        return reinforce ? Memory.RecallAndReinforce(key, sim) : Memory.Recall(key, sim);
    }

    // Self-assessed probability that the action just taken is rewarded, in (0, 1): its expected
    // value mapped from [-1, 1] to [0, 1], shrunk towards 1/2 by the self-model's prospective
    // estimate of the value error (made before the reward is seen). A deliberately exploratory
    // pick the agent expects to fail scores low however sure it is.
    double Confidence() const {
        const double certainty = 1.0 / (1.0 + std::sqrt(LastErr));
        return 0.5 + 0.5 * std::clamp(ExpectedValue(), -1.0, 1.0) * certainty;
    }
    double ExpectedValue() const { return LastAction >= 0 && LastQ.size() ? LastQ[LastAction] : 0.0; }
    int ValueResets() const { return Resets; }
    const Eigen::VectorXd& EchoState() const { return Echo.State(); }

    // Speak through the attached language model (empty if none is attached).
    std::shared_ptr<const EchoLanguageModel> Voice;
    std::string Say(const std::string& prompt, int count, std::mt19937& rng) const {
        return Voice ? Voice->Generate(prompt, count, rng) : std::string();
    }

    const Eigen::VectorXd& LastEpisodeKey() const { return CycleTrace; }
    long CyclesCompleted() const { return Cycles; }
    int Consolidations() const { return ConsolidationCount; }

    int CurrentStep() const { return Step; }

    EchoReservoir Reservoir;
    EchoReservoir Echo;      // memory channel: bias-free cycle reservoir
    EchoReservoir Local;     // cycle frame: small ring cleared at each cycle start
    GTAngelCore   Core;
    Toroid        Torus;
    EchoSpace     Memory;
    ResonanceLog  Resonance;
    NanEchoSpec   Language;

private:
    static ReservoirConfig MakeCfg(int inputDim, unsigned seed) {
        ReservoirConfig c;
        c.Size = GTAngelCore::ReservoirSize;
        c.InputDim = inputDim;
        c.Seed = seed;
        return c;
    }
    static ReservoirConfig MakeEchoCfg(int inputDim, unsigned seed) {
        ReservoirConfig c;
        c.Shape = Topology::Cycle; c.Size = EchoSize; c.InputDim = inputDim;
        c.LeakRate = 1.0; c.SpectralRadius = 0.99; c.InputScale = 0.1; c.BiasScale = 0.0; c.Seed = seed + 7;
        return c;
    }
    static ReservoirConfig MakeLocalCfg(int inputDim, unsigned seed) {
        ReservoirConfig c = MakeEchoCfg(inputDim, seed);
        c.Size = LocalSize; c.Seed = seed + 11;
        return c;
    }
    // [echo; frame; main; 1] — the cue survives the delay in this joint state (a supervised
    // readout decodes a 4-level cue from it at 0.99 ten steps later).
    static int Features() { return EchoSize + LocalSize + GTAngelCore::ReservoirSize + 1; }
    Eigen::VectorXd AgentFeatures() const {
        Eigen::VectorXd f(Features());
        // The frame ring moves its content one unit per Perceiving step; un-rotating by the
        // Perceiving steps taken this cycle (m) puts slot j of this cycle on fixed coordinates.
        const int m = (Step - 1) / 3;
        const Eigen::VectorXd& y = Local.State();
        Eigen::VectorXd aligned(LocalSize);
        for (int i = 0; i < LocalSize; ++i) aligned[i] = y[(i + m) % LocalSize];
        // both rings run at small gain (near-linear); rescale them to the main state's range
        f << Echo.State() * 10.0, aligned * 10.0, Reservoir.State(), 1.0;
        return f;
    }
    // Value: recursive least squares with one shared inverse covariance P (the state statistics
    // do not depend on the action) and a weight row per action, updated for the action taken.
    // Forgetting (ValueForget < 1) keeps P open, so values re-adapt after the world changes.
    // Self-model: NLMS on the log squared value error. Both use the error before this update.
    void LearnValue(double reward) {
        if (LastF.size() == 0) return;
        const double err = reward - LastQ[LastAction];
        if (ChangeThreshold > 0) {
            Ph[LastAction] += -err - ChangeDrift;
            PhMin[LastAction] = std::min(PhMin[LastAction], Ph[LastAction]);
            if (Ph[LastAction] - PhMin[LastAction] > ChangeThreshold) {
                Wq.setZero();
                Ph.fill(0.0); PhMin.fill(0.0); ++Resets;
                return;
            }
        }
        const Eigen::VectorXd Pf = P * LastF;
        const Eigen::VectorXd k = Pf / (ValueForget + LastF.dot(Pf));
        Wq.row(LastAction) += err * k.transpose();
        P.noalias() -= k * Pf.transpose();   // rank-1 downdate in place
        P /= ValueForget;
        Wm.row(LastAction) += 0.2 * (std::log(err * err + 1e-6) - Wm.row(LastAction).dot(LastF))
                              / (LastF.squaredNorm() + 1e-9) * LastF.transpose();
    }

    int SampleSoftmax(const Eigen::VectorXd& logits, int nActions) {
        Eigen::VectorXd p = (logits.head(nActions).array() - logits.head(nActions).maxCoeff()).exp();
        return std::discrete_distribution<int>(p.data(), p.data() + nActions)(Rng);
    }

    void EndCycle() {
        // Salience: hemispheric coherence × how strongly rewarded the cycle was
        // (mean |reward| ∈ [0,1]); neutral cycles keep half weight so they are not all tied.
        const double salience = Torus.Coherence() * (0.5 + 0.5 * std::min(1.0, CycleRewardAbs / 12.0));
        CycleRewardAbs = 0;
        if (Torus.Coherence() > 0.5) {
            Memory.Store(CycleTrace, (int)Cycles, EchoSpace::Kind::Episodic, salience);
            if (++StoresSinceConsolidation >= ConsolidateEvery && Memory.Consolidate()) {
                StoresSinceConsolidation = 0;
                ++ConsolidationCount;
            }
        }
        ++Cycles;
    }

    int Step = 0;
    int LastAction = -1;
    long Cycles = 0;
    int StoresSinceConsolidation = 0;
    int ConsolidationCount = 0;
    double CycleRewardAbs = 0;
    Eigen::MatrixXd Wq, P;
    Eigen::MatrixXd Wm;                 // self-model: one row per action
    Eigen::VectorXd LastF, LastQ;
    double LastErr = 1.0;
    int Resets = 0;
    std::array<double, GTAngelCore::ActionCount> Ph{}, PhMin{};
    std::mt19937 Rng;   // action sampling (integrated mode)
    // 4 Perceiving steps per cycle (1,4,7,10) → episode key of 4 × ReservoirSize.
    Eigen::VectorXd CycleTrace = Eigen::VectorXd::Zero(4 * GTAngelCore::ReservoirSize);
};

} // namespace u9n
