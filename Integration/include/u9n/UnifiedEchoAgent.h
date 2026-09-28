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
#pragma once

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

    explicit UnifiedEchoAgent(int inputDim = 1, unsigned seed = 42, size_t memoryCapacity = 512)
        : Reservoir(MakeCfg(inputDim, seed)), Core(seed + 1) {
        Memory.Capacity = memoryCapacity;
    }

    // One cycle step. Returns chosen action on Acting steps, -1 otherwise.
    int Tick(const Eigen::VectorXd& input, double reward, int nActions = GTAngelCore::ActionCount) {
        Step = Step % 12 + 1;
        const Stream s = Stream((Step - 1) % 3);
        int action = -1;
        switch (s) {
        case Stream::Perceiving:
            Reservoir.Step(input);
            Core.UpdateAttention(Reservoir.State());
            CycleTrace.segment(((Step - 1) / 3) * GTAngelCore::ReservoirSize, GTAngelCore::ReservoirSize) = Reservoir.State();
            break;
        case Stream::Acting:
            if (LastAction >= 0) Core.UpdateThompson(LastAction, reward);
            action = LastAction = Core.ThompsonSample(Core.GatedLogits(Reservoir.State()), nActions);
            break;
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
    int RecallEpisode(const Eigen::VectorXd& key, double* sim = nullptr) const { return Memory.Recall(key, sim); }

    const Eigen::VectorXd& LastEpisodeKey() const { return CycleTrace; }
    long CyclesCompleted() const { return Cycles; }
    int Consolidations() const { return ConsolidationCount; }

    int CurrentStep() const { return Step; }

    EchoReservoir Reservoir;
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
    // 4 Perceiving steps per cycle (1,4,7,10) → episode key of 4 × ReservoirSize.
    Eigen::VectorXd CycleTrace = Eigen::VectorXd::Zero(4 * GTAngelCore::ReservoirSize);
};

} // namespace u9n
