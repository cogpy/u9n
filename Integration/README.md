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
| Nonlinear temporal reasoning (3-bit parity) | 1.00 | **Improved** from 0.95 (accuracy 0.977 → 1.000). The default leak of 0.3 blurs consecutive ±1 symbols together and destroys the sign products parity needs. The probe now picks leak, gain, ρ and the quadratic readout on validation data, ranking candidates by parity 3, 5 and 7 together because parity 3 alone saturates. It chose leak 1, gain 2, ρ 0.5 with `[x; x²]`. Because the 3-bit score is now saturated, the note reports accuracy for each parity order. Parity up to 7 bits is solved (0.96 at 7 bits), and 8 bits drops to chance. With default settings 5-bit parity reaches 0.71. |
| Closed-loop contextual agency | 0.99 | **Improved** from 0.94 by the integrated agent (see below): its own reward learning now adds to the supervised imitation signal it still receives through Wout. The score is the mean of 3 seeds, because single runs vary by several points. Reward rate is 0.891 with integration on and 0.866 with it off; on 6 seeds integration wins every time, and 0.90 is the ceiling. The cycle frame below moved the 6-seed mean from 0.891 to 0.893, within seed noise. |
| Integrated agency (one agent, one episode) | 0.96 | **Improved** from 0.79 by the cycle frame (see below). The only probe that tests the whole agent without help. A cue appears once per 12-step cycle, followed by noise; the agent must act on it ten steps later, learning from reward alone, and the cue-to-action rule rotates halfway through. Mean of 3 seeds: late-window accuracy on delayed decisions is 0.97 both before and after the rotation (it was 0.83 and 0.85); with integration off it is 0.27 for both (chance 0.25). Every run recovered after the rotation, in 114 cycles on average (230 before), with 2 detected change points per run. Confidence, now the agent's own estimate of the chance its action is rewarded, separates correct from wrong decisions with AUROC 0.81 (0.70 before). |
| Adaptive decision-making (non-stationary bandit) | 0.92 | **Improved** from 0.79. Regret per run fell from 312 to 125. The probe swaps constant discounting (γ=0.995, which forgets all the time and over-explores) for a per-arm Page-Hinkley change detector in `GTAngelCore`. When a detected drop in reward arrives, it resets the posteriors. Over 20 runs it fired 19 times, each run having one real regime change. The detector is off by default. |
| Metacognition (predicted vs true error) | 0.95 | **Improved** from 0.77. `GTAngelCore` now has a forward-looking self-model: a second readout over `[x; x²; 1]` that predicts, from the current state alone, the log squared error the main readout is about to make, before it sees the target. Its predicted error correlates with actual error at r = 0.95. Coherence managed only r = −0.77, because it averages loss the core has already seen. A unit test checks that the self-model expects about 200× more error in states whose targets are noisy. |
| Working memory (MC) | 1.00 | **Improved** from 0.82 (it was 0.37 on the same MC/150 scale before the cycle reservoir). MC is 149.8 of 150 delays. The default per-unit bias of 0.1 pushes units off tanh's linear centre, which capped the cycle reservoir at 123. The new `ReservoirConfig::BiasScale` (default still 0.1) is chosen on validation data, and the probe picks 0. Because the 150-delay score now saturates, the note also reports capacity over the full 1..512 delays: 434 of a theoretical N = 512, against about 123 with bias. |
| Temporal prediction (NARMA-10) | 0.98 | **Improved** from 0.90 (NRMSE 0.078 → 0.013, against 0.81 for the linear baseline) by a **deep echo** cascade. NARMA-10 is a recursion, y(t+1) = 0.3y + 0.05y·Σy + 1.5u(t−9)u(t) + 0.1. Through its y·Σy term the map from inputs is of higher order than a quadratic readout of input-driven states can express, and every single-reservoir variant plateaued near 0.08 on validation: random or cycle topology, any leak, gain, bias or spectral radius, and a cubic readout. Level k of the cascade is a reservoir driven by [u, ŷ_{k−1}], the estimate read out from levels 0..k−1. That readout is fitted on the fit slice only, so nothing downstream sees validation or test targets. Each level can then form products of the running estimate with itself and with the input. Validation NRMSE by depth is 0.087, 0.030, 0.019 and 0.016, and the probe picks the depth on validation (4, the cap). Levels above the first have 256 units: 512 cut test NRMSE only from 0.013 to 0.011, at four times the solve cost. Leak, gain and ridge are still selected on validation as before (leak 1, gain 0.05). |
| Episodic memory (noisy cue recall) | 0.90 | **Improved** from 0.75. The memory now picks its own encoder by rehearsing its stored episodes under simulated noise (σ=0.35, a separate random stream; the test cues are never seen). It chose a near-isometric cycle reservoir, ρ 0.8, with plain cosine. Recall is 1.00 at σ=0.25 and 0.835 at σ=0.5, equal to cosine matching on the raw input sequences with no reservoir, so the encoding now loses nothing. Whitening hurts with this encoder: 200 keys cannot estimate a 10,240-dimension covariance. The scale's ideal of 1.0 is out of reach at σ=0.5, because even the raw input sequences give only 0.835. |
| Continual learning (3 sequential tasks) | 0.96 | **Fixed** with `ContextualReadout`. Worst-task NRMSE is 0.04; the single GTAngel Wout scores 0.75 on the same run. See below. |
| Selective attention / toroid phase lock | 1.00 | These pass by construction: they confirm the dynamics are correct, not that the system is intelligent. |
| Language modelling (character-level, EchoSelf corpus) | 0.85 | **Improved** from 0.83 (0.842 → 0.737 bits per char). The two experts are now combined geometrically rather than linearly: log p(c) = g_e·log p_echo(c) + g_n·log p_counts(c) − log Z, a product of experts. Where the experts agree the product sharpens; where one is unsure, the weights flatten it. One weight pair g_d is kept for each matched context depth d. It starts at the best linear weight, is fitted by gradient descent on validation text, and keeps descending while reading the test text (predict first, then step), just as the counts keep counting. Under the product blend a slower echo helps, so the probe now picks the reservoir leak on validation: it calibrates on half the validation pairs, scores the other half, and chose 0.3 from {0.2, 0.3, 0.5, 1}. On held-out text the linear blend scores 0.842, the reservoir alone 1.64, the n-gram alone 1.13 and character frequencies 4.88. Most of the remaining loss is at novel word starts (measured with the leak-1 echo): 72% of characters fall in a context of 12 or more seen characters and cost 0.26 bits each, while the 11% with a matched context of 2–4 characters cost about 3 bits each. Four variants were tried and dropped; none gained more than 0.001 bits per char on validation: extra n-gram experts capped at orders 2, 4 and 7; a word-context expert (previous words plus the current word prefix); mixer weights also keyed on expert confidence; and n-gram order 16 or 24. Earlier rounds added order-12 sparse hashed counts and online counting. The NanEcho 48M transformer itself still cannot be evaluated (no public checkpoint). This probe measures next-character prediction, not open-ended reasoning. |

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

### Integrated agent

Until this change every improvement lived in a probe: each one tuned its own reservoir, detector or readout, while `UnifiedEchoAgent` ran the defaults and learned nothing from reward on its own. Now the parts run inside the agent's 12-step cycle (`Integrated = true` by default):

- **Memory channel.** A 256-unit bias-free cycle reservoir is stepped with the main reservoir on Perceiving steps. A supervised readout decodes a 4-level cue from [echo; main] ten steps after it was shown with 0.99 accuracy; the main reservoir alone manages 0.90.
- **Cycle frame.** The memory channel's ring moves each input one unit along per Perceiving step. Within a cycle, the cue therefore sits on different coordinates at each of the four Acting steps, and one shared value readout had to find it at all four. A second, 16-unit ring is now cleared at the start of each cycle and read un-rotated by the cycle phase (the Perceiving steps taken so far). Slot j of the current cycle then lands on the same coordinates at every Acting step. A cue shown ten steps ago sits in slot 0, and so does a cue that is still present. The 12-step cycle becomes the agent's own reference frame. See the frame round below.
- **Value learning from reward.** Q = Wq·[echo; frame; main; 1] is fitted by recursive least squares on the action taken. One inverse covariance is shared across actions, and a forgetting factor of 0.999 keeps it adaptive. Actions are sampled from softmax((gated logits + Q) / T). An earlier NLMS version reached only 0.53 accuracy on the integrated probe: with about 770 correlated features it converged too slowly.
- **Change points.** A per-action Page-Hinkley test runs on the agent's own value shortfall Q_a − r (threshold 40, drift 0.8). An alarm clears Wq but keeps the state covariance P, because the inputs did not change, only the rule. Over 6 seeds the detector lifts integrated accuracy from 0.70 to 0.82. Thresholds of 20 or lower set off alarm storms. Reusing GTAngel's context-free Beta detector here fired 30 times in one run, because inside a contextual task every context switch looks like a change.
- **Self-model.** One row per action predicts the log squared value error before reward arrives. `Confidence()` combines it with Q into the probability that the action taken is rewarded: 0.5 + 0.5·clamp(Q, −1, 1)·certainty, where certainty = 1 / (1 + predicted error). An exploratory pick the agent expects to fail now scores low, however sure the agent is of that. Before, confidence tracked only the value error, so a confidently bad choice looked confident. The exploration temperature (0.01 to 0.3) rises when the agent is unsure of its best option. A single state-level row gave AUROC 0.52; per-action rows give 0.66.
- **Voice.** An optional `EchoLanguageModel` can be attached (`Voice`), and `Say(prompt, n, rng)` speaks through it. Nothing is scored on it yet.

Unit tests cover reward-only learning and relearning after a rule flip: 0.99 / 1.00 with integration on, 0.52 with it off, and exactly one detected change. A delayed-cue test (4 cues, cue then noise, 3 seeds) needs 0.78 accuracy at step 11 and reaches 0.83; without the frame it reached 0.67. The same test checks that confidence is 0.18 higher on correct decisions than on wrong ones; the old confidence managed 0.04. The tests also cover the voice. No existing probe score dropped. Settings were chosen on 6-seed means: an earlier single-seed tuning turned out to be partly seed luck. The evaluation runs its probes, and the seeds within a probe, concurrently. Each one seeds its own random generator, so the output is identical to a sequential run. On 4 cores the full evaluation takes about 1.5 minutes; sequentially it took about 3 minutes.

#### Tuning round (6-seed means on the integrated probe)

| Change | Mean accuracy | Recovery (cycles) |
|---|---:|---:|
| As merged | 0.820 | 858 |
| Keep P on reset | 0.823 | 690 |
| + exploration 0.01–0.3, drift 0.8 (adopted) | 0.851 | 198 |

Tried and not adopted:
- **Counterfactual push.** A win also pushes the other actions' values toward −1, which reached 0.895 with recovery in 453 cycles. It assumes exactly one action pays per state. That holds for both agent probes, so turning it on by default would be teaching to the test.
- **Learning that assumption online.** Measured from the agent's own values, the estimate fed back on itself: the push lowers the values it is measured on, and in a noisy task the gate turned on. Measured from observed win rates, it was biased: the agent explores mostly where it is unsure, so an exclusive task looked non-exclusive.
- **Fewer features.** Thinning the main-reservoir part of the features did not help.
- **Smaller echo spectral radius.** Dropping it to 0.9 made both tasks worse.

#### Frame round (6-seed means)

| Variant | Integrated agency | Closed-loop reward rate | Reward-only test, late accuracy |
|---|---:|---:|---:|
| Previous agent | 0.850 | 0.891 | 0.97 / 1.00 |
| Whole echo ring un-rotated by phase | 0.953 | 0.51 (3 seeds) | fails |
| Raw and un-rotated echo side by side | 0.947 | 0.887 (3 seeds) | 0.85 / 0.87 |
| Cycle frame, 64 units | 0.962 | 0.883 | 0.99 / 1.00 |
| Cycle frame, 32 units | 0.967 | 0.889 | 0.99 / 1.00 |
| **Cycle frame, 16 units (adopted)** | **0.973** | **0.893** | **0.99 / 1.00** |
| Cycle frame, 8 units | 0.965 | 0.895 | 1.00 / 1.00 |
| Cycle frame, 4 units | 0.918 | 0.895 | 1.00 / 1.00 |

Un-rotating the whole long-memory ring aligns the delayed cue, but it breaks inputs that persist. A held cue becomes a different vector at each phase, and the agent settled on actions it valued at −0.8 without ever trying the right one. Keeping the raw ring beside the rotated one fixed the closed loop but not the reward-only test. With 1,025 features, relearning after the flip was slow. A small ring cleared every cycle has neither problem: it holds only this cycle's slots, all at fixed coordinates. With the frame, each run has two change alarms: one during warm-up and one 69–93 cycles after the rotation. Recovery takes 114 cycles on average. Faster value forgetting (0.998, 0.9985) made the integrated probe much worse (0.63–0.66), so 0.999 stays.

**Persistent inputs (corrected).** The previous round listed persistent inputs as a limitation, which overstated it. A constant input does pile up in the memory channel: with ρ 0.99 the echo norm reaches about 37, against about 6 for the main state. In a noisy-reward test, the value *magnitudes* were then biased. Decisions are not hurt, though. With the cue held through the whole cycle, the agent reaches 0.987 accuracy on 6 seeds, because the cue is still present when it acts. Three remedies for the echo channel were tried: change coding, input mean removal, and feature standardisation. On the cue-then-noise probe they score 0.849–0.854, against 0.851 for the current design. On the persistent-cue task they score 0.892–0.969, against 0.987. The echo channel is therefore unchanged.

### Verdict

Deep Tree Echo is **not AGI, and nowhere near it**. It is a well-integrated **narrow adaptive-control stack**.
The reservoir, ECAN, Thompson sampling and toroid are all real mechanisms. On small temporal and decision tasks
they perform at or above textbook baselines.

The capabilities that define general intelligence are absent or failing:

- **Open-ended language and reasoning** is not evaluated. The 48 M NanEcho checkpoint is unavailable, and the language probe measures character prediction only.
- **Episodic memory** is now at its information ceiling (0.835 recall at σ=0.5, equal to matching on the raw sequences), but recall is tested on 20-step synthetic sequences, not lived experience.
- **Abstraction and transfer across domains** has no mechanism at all.
- **Planning over long horizons** has no model-based search.

Most of the documentation's "Complete" labels describe architecture that exists, not capability that has been measured.

### Highest-leverage next steps

1. **Language.** Load a trained NanEcho checkpoint and report perplexity. The agent already has a `Voice` slot, so bind the checkpoint there and score what the agent says about its own state.
2. **UE bridge.** Wrap `UnifiedEchoAgent` in a `UActorComponent` under `DeepTreeEcho/`, and mirror GTAngel's `Ue5PlayerAiBridgeService` IPC so the WPF trainer and UE5 share one core.
