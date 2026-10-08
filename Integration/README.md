# Integration — GTAngelEcho + EchoSelf-48M → u9n

This folder puts the two sibling repositories on the same Deep Tree Echo reservoir
substrate as u9n. It is a header-only C++17 layer. It depends only on Eigen, so it
builds and tests anywhere, with no Unreal, WPF or PyTorch. Unreal components can
wrap these classes directly.

| Upstream | Upstream artefact | u9n port | File |
|---|---|---|---|
| `cogpy/GTAngelEcho` @191300d | `DteCognitiveCoreService.cs`: ECAN, MOSES, Wout, Thompson, coherence | `u9n::GTAngelCore` (same constants: 512 neurons, 16×32 clusters, 18 actions) | `include/u9n/GTAngelCore.h` |
| `9cog/EchoSelf-48M` @272cdcb | `toroidalCognitiveService.ts`: DTE ⟷ Marduk hemispheres | `u9n::Toroid` (Kuramoto pair, anti-phase lock) | `include/u9n/EchoSelfCore.h` |
| | `echo/hypergraph.scm` `adaptive-attention` | `u9n::AdaptiveAttention` (same formula) | 〃 |
| | `echoSpaceService.ts`: vector memory | `u9n::EchoSpace` (cosine recall, salience weighted) | 〃 |
| | `deepTreeEchoService.ts` `getImprovementTrend` | `u9n::ResonanceLog` | 〃 |
| | NanEcho `train_nanecho.py` (8L/8H/512d) | `u9n::NanEchoSpec` (descriptor only, no weights) | 〃 |
| u9n | continual-learning readout bank | `u9n::ContextualReadout` (context-routed heads, no task labels) | `include/u9n/ContextualReadout.h` |
| u9n | ESN (ρ=0.9, leak=0.3) | `u9n::EchoReservoir` (ridge readout) | `include/u9n/EchoReservoir.h` |
| **all** | 12-step / 3-stream cycle | `u9n::UnifiedEchoAgent` | `include/u9n/UnifiedEchoAgent.h` |

`UnifiedEchoAgent::Tick` runs the CLAUDE.md cycle as follows:

- **Perceiving** steps (1, 4, 7, 10) drive the reservoir and ECAN attention.
- **Acting** steps (2, 5, 8, 11) produce STI-gated logits and a Thompson action.
- **Reflecting** steps (3, 6, 9, 12) run MOSES mining, the toroid and the resonance log.
- At the end of each cycle (step 12), the four perceived states form one episode, which EchoSpace stores when the hemispheres are coherent. EchoSpace re-fits its whitening every `ConsolidateEvery` stored episodes (see "Episodic memory in the agent cycle" below).

## Build / run

```bash
cmake -B build && cmake --build build --target U9nIntegrationTests DeepTreeEchoAGIEval
ctest --test-dir build -R U9nIntegrationTests       # labelled "unit", so CI's `ctest -L unit` runs it
./build/bin/DeepTreeEchoAGIEval report.md report.json   # ~40 s
```

## AGI capability evaluation

The raw numbers are in [`AGI_EVALUATION_RESULTS.md`](AGI_EVALUATION_RESULTS.md), produced by `eval/agi_eval.cpp`
(deterministic seeds). Each probe gives a score equal to the fraction of the gap between a trivial baseline and the ideal that the system closes.

| Capability | Score | Reading |
|---|---:|---|
| Nonlinear temporal reasoning (3-bit parity) | 0.96 | Strong. The reservoir really does compute nonlinear functions of the past. |
| Closed-loop contextual agency | 0.94 | Strong, but the agent gets a supervised imitation signal through Wout. |
| Adaptive decision-making (non-stationary bandit) | 0.79 | Good. Discounted Thompson recovers after the regime shift. |
| Metacognition (coherence vs true error) | 0.77 | r = −0.77. Coherence tracks competence, but it is built partly from the same loss, so this overstates introspection. |
| Working memory (MC) | 0.55 | MC ≈ 55 of a 512 ceiling, which is typical for tanh ESNs. |
| Temporal prediction (NARMA-10) | 0.90 | **Improved** from 0.39. NRMSE 0.078 against 0.81 for the linear baseline. The probe uses a quadratic readout `[x; x²]` and picks leak, input gain and ridge on a validation slice of the training data. The default leak of 0.3 blurs NARMA's 10-step lag window; validation picks leak 1.0. |
| Episodic memory (noisy cue recall) | 0.75 | **Improved** from 0.27. Recall is 1.00 at cue noise σ=0.25 and 0.59 at σ=0.5; the old path scored 0.28 and 0.08. The score is measured from the old path as baseline. See below. |
| Continual learning (3 sequential tasks) | 0.96 | **Fixed** with `ContextualReadout`. Worst-task NRMSE is 0.04; the single GTAngel Wout scores 0.75 on the same run. See below. |
| Selective attention / toroid phase lock | 1.00 | These pass by construction: they confirm the dynamics are correct, not that the system is intelligent. |
| Language & open-ended reasoning (NanEcho 48M) | N/A | No checkpoint or torch runtime here. At about 51 M parameters it would be a small nanoGPT at best. |

### Continual learning fix

`ContextualReadout` replaces the single online readout with a bank of readout heads:

- **Context key.** The key is a slow running average of each neuron's squared activation, which gives a label-free signature of what the reservoir is currently doing.
- **Routing.** Each sample goes to the head whose prototype is closest to the key by cosine similarity. A new head spawns, warm-started from the nearest one, when no prototype is closer than 0.97.
- **Learning.** Only the routed head takes the GTAngel ridge-SGD step, so heads never overwrite each other.
- **Settling gate.** Training waits until the key has integrated about 150 steps. Without this gate, the first samples of a new task were written into the old task's head, and the fix barely helped.

Probe 7 learns three sine-prediction tasks in sequence (frequencies 0.2, 0.05 and 0.11), then tests all three:

| Task | Single Wout NRMSE | ContextualReadout NRMSE |
|---|---:|---:|
| A (0.2) | 0.75 | 0.010 |
| B (0.05) | 0.48 | 0.040 |
| C (0.11) | 0.02 | 0.022 |

The limits of the fix:

- It works when tasks drive the reservoir into distinguishable states.
- Two tasks that share an input signature but need different outputs would still collide in one head.
- It does not produce transfer between tasks.

### Episodic memory fix

The old path stored the final reservoir state as the key and recalled by raw cosine. It failed for two reasons, each measured separately:

1. **The final state forgets the episode.** With leak rate 0.3 and ρ = 0.9, the last state is dominated by the last few inputs. `EchoReservoir::EncodeTrajectory` instead concatenates the states along the trajectory. The final state alone recalled 0.28, five sampled states 0.68, and all 20 states 0.89.
2. **Shared directions swamp the cosine.** Every reservoir state shares a large common subspace, from the bias and input-driven modes. `EchoSpace::Consolidate()` fits a shrunk PCA whitening on the stored keys only, then projects both stored keys and cues through it. With whitening, the full trajectory recalls 0.995–1.00.

Two details in `Consolidate()` matter:

- **Numerically zero components are dropped.** Centring leaves one component with eigenvalue ≈ 0, and whitening it amplifies round-off without bound. That bug cost recall until it was fixed.
- **The shrinkage ridge is α·√(σ_max·σ_median).** Scaling to σ_max alone switches whitening off when one shared mode dominates (the unit test covers this case). Scaling to σ_median alone amplifies cue noise on steep reservoir spectra. The geometric mean works for both.

Recall with 200 stored 20-step sequences:

| Cue noise | Final state + raw cosine | Trajectory + consolidated |
|---|---:|---:|
| σ = 0.25 | 0.275 | 1.00 |
| σ = 0.5 | 0.075 | 0.59 |

The limits of the fix:

- **Keys stored after a fit.** Projecting only onto the fitted span would collapse a later key that points outside it; an orthogonal key mapped to zero and could not be recalled until the next consolidation. The out-of-span residual is kept as extra coordinates, weighted like an in-span direction of median variance (`ResidualWeight`, default 1). Probe recall and recall of keys stored after a fit are identical for weights 0–2, and the orthogonal case is fixed for any weight above 0.
- **High noise.** At σ = 0.5, noise is as large as the signal, and recall falls to 0.59. A shrinkage scaled only to σ_max reached 0.82 there but broke the dominant-shared-mode case. Getting both would need the shrinkage chosen per memory store (e.g. by held-out reconstruction) rather than one global constant.
- **Consolidation is a batch step.** It costs O(M²·D + M³), so the agent amortises it instead of running it on every store (see below).

### Episodic memory in the agent cycle

`UnifiedEchoAgent` now uses the same trajectory keys and consolidation as the probe:

- **Episodes.** At step 12, the four Perceiving states of the cycle (steps 1, 4, 7, 10) are concatenated into one episode key, labelled with the cycle index. The key is stored only when the toroid's coherence is above 0.5. This replaces storing a single raw state at every sync point, which produced about 3,000 unconsolidated entries per 12,000 ticks.
- **Bounded memory.** `EchoSpace::Capacity` defaults to 512 episodes in the agent. On overflow it evicts the entry with the lowest retention, `salience · RetentionDecay^age`, where age counts stores since the entry was written. The agent sets salience to hemispheric coherence × (0.5 + 0.5·mean |reward| of the cycle), so strongly rewarded or punished cycles outlive neutral ones.
- **Sleep consolidation.** Every `ConsolidateEvery` stored episodes (default 128), the agent calls `Consolidate()` at the end of the cycle. Episodes stored in between are projected through the latest fitted map.
- **Recall.** `RecallEpisode(key)` returns the cycle index of the nearest stored episode. `LastEpisodeKey()` exposes the current cycle's key.
- **Recall reinforces salience.** A confident `RecallEpisode` (the default; pass `reinforce=false` for a pure lookup) calls `EchoSpace::RecallAndReinforce`. It boosts the episode's salience by `s ← s + 0.2·(1 − s)` and resets its age, so episodes the agent keeps using resist eviction. `EchoSpace::Recall` remains a pure lookup, so the evaluation probes are unaffected.

Salience-weighted eviction was measured on a closed-loop run: 1,500 cycles, capacity 128, with rewards only during one third of the time ("eventful" cycles). The table shows what fraction of retained episodes were eventful, and the oldest cycle still held:

| `RetentionDecay` | Eventful fraction kept | Oldest cycle kept |
|---|---:|---:|
| 0 (equivalent to FIFO) | 0.30 (base rate) | 1,372 |
| 0.995 | 0.57 | 1,277 |
| **0.998 (default)** | **0.94** | 1,170 |
| 0.999 | 0.99 | 1,103 |

Higher decay keeps more salient episodes but reaches further into the past. At 1.0, pure salience ranking kept an episode from cycle 6. The right value scales with capacity, so treat 0.998 as tuned for capacities of a few hundred.

Reinforcement only fires on a confident match, so a false recall cannot entrench itself. Choosing the confidence test took three attempts, each measured on agent episodes: cues from episodes still in memory should pass, and cues from episodes already evicted should not.

| Confidence test | Stored-episode cues passing | Evicted-episode cues passing |
|---|---:|---:|
| Absolute cosine ≥ 0.3 | Real matches score 0.19–0.52; white-noise cues reach 0.37 | — |
| Best-vs-runner-up margin ≥ 0.25 | 0.55 | 0.20 |
| z-score ≥ 3.5 | 0.84 | 0.31 |
| **z-score ≥ 3.75 (default)** | **0.75** | **0.16** |
| z-score ≥ 4.0 | 0.61 | 0.09 |

- **Why not absolute cosine.** Cosine levels shift with the whitening and the key size, so no fixed cut-off transfers between stores.
- **The z-score.** It measures how far the best match stands out from all stored entries: `(best − mean)/std`. It cannot exceed √(n−1) for n entries, so small stores use 0.9·√(n−1) as the threshold.

To measure the effect, the agent ran 1,500 cycles at capacity 128 and recalled 10 early episodes every 20 cycles:

| Recall mode | Tracked episodes still stored | Recall accuracy |
|---|---:|---:|
| Pure lookup | 0 / 10 | 0.90 |
| Reinforcing recall | 8 / 10 | 0.94 |

In the closed-loop probe (12,000 ticks), the agent fills its 512 slots and consolidates 7 times. That adds about 4 s to the evaluation and leaves every score unchanged.

### Verdict

Deep Tree Echo is **not AGI, and nowhere near it**. It is a well-integrated **narrow adaptive-control stack**.
The reservoir, ECAN, Thompson sampling and toroid are all real mechanisms. On small temporal and decision tasks
they perform at or above textbook baselines.

The capabilities that define general intelligence are absent or failing:

- **Open-ended language and reasoning** is not evaluated, and the only candidate is a 48 M model.
- **Episodic memory** degrades at high cue noise (0.59 recall at σ=0.5).
- **Abstraction and transfer across domains** has no mechanism at all.
- **Planning over long horizons** has no model-based search.

Most of the documentation's "Complete" labels describe architecture that exists, not capability that has been measured.

### Highest-leverage next steps

1. **Language.** Load a trained NanEcho checkpoint, report perplexity, and bind it to `UnifiedEchoAgent` through the Reflecting stream.
2. **UE bridge.** Wrap `UnifiedEchoAgent` in a `UActorComponent` under `DeepTreeEcho/`, and mirror GTAngel's `Ue5PlayerAiBridgeService` IPC so the WPF trainer and UE5 share one core.
