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
- At sync points {1, 5, 9}, EchoSpace consolidates the state when the hemispheres are coherent.

## Build / run

```bash
cmake -B build && cmake --build build --target U9nIntegrationTests DeepTreeEchoAGIEval
ctest --test-dir build -R U9nIntegrationTests       # labelled "unit", so CI's `ctest -L unit` runs it
./build/bin/DeepTreeEchoAGIEval report.md report.json   # ~45 s
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
| Temporal prediction (NARMA-10) | 0.39 | NRMSE 0.50 against 0.82 for the linear baseline. A tuned ESN reaches about 0.2–0.4. |
| Episodic memory (noisy cue recall) | 0.27 | Weak. Reservoir encodings of 20-step sequences are not noise-robust keys. |
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

### Verdict

Deep Tree Echo is **not AGI, and nowhere near it**. It is a well-integrated **narrow adaptive-control stack**.
The reservoir, ECAN, Thompson sampling and toroid are all real mechanisms. On small temporal and decision tasks
they perform at or above textbook baselines.

The capabilities that define general intelligence are absent or failing:

- **Open-ended language and reasoning** is not evaluated, and the only candidate is a 48 M model.
- **Episodic memory** is weak.
- **Abstraction and transfer across domains** has no mechanism at all.
- **Planning over long horizons** has no model-based search.

Most of the documentation's "Complete" labels describe architecture that exists, not capability that has been measured.

### Highest-leverage next steps

1. **Episodic keys.** Use EchoSpace keys from the time-averaged, attention-weighted state rather than the final state.
2. **Language.** Load a trained NanEcho checkpoint, report perplexity, and bind it to `UnifiedEchoAgent` through the Reflecting stream.
3. **UE bridge.** Wrap `UnifiedEchoAgent` in a `UActorComponent` under `DeepTreeEcho/`, and mirror GTAngel's `Ue5PlayerAiBridgeService` IPC so the WPF trainer and UE5 share one core.
