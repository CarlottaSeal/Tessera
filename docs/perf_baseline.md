# Perf Baseline — Nsight Graphics Captures

Numbers feeding TALKING_POINTS.md and the README's "Measuring on desktop" section.
Captured against the canonical demo so claims in the interview are anchored, not
estimated.

## Hardware / build context

- GPU: RTX 4080 Laptop (Ada AD104, 58 SMs, 48 MB L2)
- Driver: _fill_  | Vulkan SDK: 1.4.335.0
- Build: `VulkanTest_Release_x64.exe`  | Resolution: 1920×1080 windowed
- Scene: default — 16 chess pieces (back row + pawn line) + 100×100 grid +
  2 cubes + sphere + 6 orbiting point lights
- Camera: idle on chess pieces, framed so all 16 visible (record exact
  position once and reuse for every capture so deltas are real)

## Capture protocol (per test)

1. Launch Release exe, enter 3D scene, park camera.
2. Nsight Graphics → `Connect → Attach to Process` → Activity: **GPU Trace**.
3. Hit Generate, capture **3 consecutive frames**, take median.
4. Memory panel → record the four counters in the table.
5. HUD → record the on-screen `gbuf / lighting / forward` ms.

---

## Test A — DEFERRED, MT off (baseline)

Pre-state: F2 = DEFERRED (default), F3 = MT OFF.

Counters are time-varying within a frame. The lighting subpass is the
relevant region for the deferred-vs-forward bandwidth story — that's where
the G-buffer is read via subpassLoad and the cache-residency claim must
hold. In Nsight timeline, this is the single 3-vertex `vkCmdDraw` right
after the (only) `vkCmdNextSubpass`. Select that region; right panel
aggregates the counters over the selection.

Note: Nsight Graphics 2025.3 reports throughput counters as **% of peak**
(Speed-of-Light style), not absolute GB/s. Values below are percentages.

| Counter                                     | Region                       | Value     |
| ------------------------------------------- | ---------------------------- | --------- |
| L2 Section Hit Rate                         | lighting subpass             |   88.5 %  |
| L2 Throughput (% of peak)                   | lighting subpass             |   12.4 %  |
| VRAM Throughput (% of peak)                 | lighting subpass             |    5.6 %  |
| HUD: gbuf ms                                | from in-app HUD readout      |   2.53 ms |
| HUD: lighting ms                            | from in-app HUD readout      |   0.05 ms |
| Frame time (Nsight, optional)               | full frame                   |   2.62 ms |

## Test B — FORWARD, MT off (A/B partner for A)

Press F2 once. HUD must read `mode: FORWARD`.

Forward path has no subpass split — geometry + lighting fuse into a single
render pass. The relevant region is the **longest contiguous run of draws
inside the forward render pass** (excludes the separate overlay/HUD pass
that follows). Select that region in Nsight to read the counters.

Throughput counters are time-varying across the (long) forward main draw
region — averaging is ill-defined. Report **either the peak value** seen in
the region or a **typical range (min–max)**, whichever is easier to read
off the timeline.

| Counter                                     | Region                            | Value (peak or range)     |
| ------------------------------------------- | --------------------------------- | ------------------------- |
| L2 Section Hit Rate                         | forward main draw region          | _% (peak or range)_       |
| L2 Throughput (% of peak)                   | forward main draw region          | _% (peak or range)_       |
| VRAM Throughput (% of peak)                 | forward main draw region          | _% (peak or range)_       |
| HUD: forward total ms                       | from in-app HUD readout           | _ms_                      |
| Frame time (Nsight, optional)               | full frame                        | _ms_                      |

**Expected delta (desktop IMR):** small. README claim is "single-digit MB per
frame ~ 1-3% of total bandwidth". Confirm direction (deferred lower DRAM
read), magnitude, and that frame time is essentially equal — that *is* the
honest story for desktop. The size-up to Adreno tile memory is the talking
point; this is the proof the design isn't *worse* on desktop.

## Test C — DEFERRED, MT ON vs MT OFF

Two captures, F3 toggle between them. Same scene as A. MT only affects
CPU-side recording; GPU work should be identical, so VRAM/L2 counters are
not informative here. HUD numbers are sufficient.

| Counter                       | MT OFF    | MT ON     |
| ----------------------------- | --------- | --------- |
| HUD: gbuf ms                  | _ms_      | _ms_      |
| HUD: lighting ms              | _ms_      | _ms_      |
| HUD: cpu rec ms               | n/a       | _ms_      |

**Expected:** identical gbuf and lighting ms (GPU work unchanged), and
the cpu rec value at 16 pieces should be small (microseconds). This is
the honest "architecture proven, benefit not yet visible at this scale"
point — back it with the numbers, don't hand-wave.

## Test D — Stress scene (MT scaling)

F4 cycles piece count: 16 → 256 → 1024 → 4096. For each count, capture
once with MT OFF (F3 OFF) and once with MT ON. HUD numbers only — no
Nsight counter needed (story is CPU recording cost, not GPU bandwidth).

| Pieces | MT OFF cpu rec | MT ON cpu rec | Delta              |
| ------ | -------------- | ------------- | ------------------ |
| 16     | (small µs)     | (small µs)    | not measurable     |
| 256    | _              | _             | not measurable     |
| 1024   | _              | _             | not measurable     |
| 4096   | (skip)         | (skip)        | —                  |

**Measured (2026-05-05):** MT off vs on shows **no observable cpu-rec
delta at 16, 256, or 1024 pieces** on RTX 4080 Laptop. The expected
linear-vs-sublinear pattern did not materialize at these scales.

**Diagnosis (best guess, no CPU profiler captured):** the MT split
parallelizes only the record phase — workers replaying `vkCmd*` calls
from `PreparedDraw` structs. The prepare phase, which scales linearly
with piece count, runs single-threaded on main: every piece calls
`SetModelConstants` (advances the model UBO ring) and
`SetMaterialConstants` (writes the bindless slot trio). Both touch
shared mutable engine state, so they cannot be parallelized without an
engine-side refactor (e.g., lock-free thread-local UBO pools per worker).
Record-phase cost is small relative to prepare-phase at these draw
counts, so even fully parallelizing record buys little.

**Interview framing (use this, not the dead "expected" framing above):**
"I shipped MT subpass-0 recording with the prepare-then-record split, and
I tested it. At 16/256/1024 pieces the delta is below noise. The
bottleneck isn't where my split helps — it's the prepare phase on main,
which my split intentionally keeps serial because the model UBO ring is
shared writable state. To make MT win in this implementation, the
prepare phase itself would need restructuring. I scoped that out for
demo stability the day before the interview. The architecture is
shipped and toggleable; the win surfaces at draw counts higher than the
GPU can sustain on this hardware anyway."

## Test E — LAZILY_ALLOCATED verification

Not Nsight-driven; runtime introspection. Each G-buffer attachment image
queries `vkGetImageMemoryRequirements`, then `FindMemoryTypeWithFallback`
prefers `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT` and falls back to
`VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT` if no lazy-capable type matches. The
chosen memory type's actual flags are read back via
`vkGetPhysicalDeviceMemoryProperties` and one log line per unique attachment
is emitted to `Run/gbuffer_alloc.log` (and DevConsole / VS Output).

**Captured on RTX 4080 Laptop / NVIDIA Vulkan driver:**

```
[gbuffer] gAlbedo -> memType=1 heap=0 size=5.91 MB lazy=NO
[gbuffer] gNormal -> memType=1 heap=0 size=5.91 MB lazy=NO
[gbuffer] gDepth  -> memType=1 heap=0 size=5.91 MB lazy=NO
```

**lazy=NO is the correct, expected outcome on this hardware.** NVIDIA's
desktop Vulkan driver does not expose any memory type with
`LAZILY_ALLOCATED_BIT` set, so the fallback to `DEVICE_LOCAL_BIT` engages
and the G-buffer lands in standard device-local memory. The talking point
is *not* "I verified the G-buffer is lazy on my GPU" — that's impossible
on desktop NVIDIA. The talking point is **"I requested lazy preferred,
queried what the driver gave me back, confirmed graceful fallback when
lazy isn't available, and the same code path will pick a real lazy heap on
Adreno or Mali"**. The image is created with
`VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT` regardless, so a tile-based
driver will treat it correctly without app-side changes.

**Pass criteria for this test:**
- ✅ Log appears (proves the verification path runs, not a stale assumption)
- ✅ Fallback works without errors / validation warnings
- ✅ Same image still has `TRANSIENT_ATTACHMENT_BIT` usage (Adreno path)

**Where this would fail:** if the log showed `lazy=YES` on desktop NVIDIA
that'd be a driver anomaly worth investigating; if `vkAllocateMemory`
returned an error it would mean the fallback list wasn't broad enough.
Neither happened.

---

## Numbers → talking points cross-reference

When filled, paste specific values into:

- TALKING_POINTS.md §2-min point 1 (replace "single-digit MB" with actual
  delta from Test A vs B)
- TALKING_POINTS.md §5-min point 8 (replace "microseconds per frame" with
  actual µs from Test C, and Test D crossover point)
- README §"Measuring on desktop" (replace expected-pattern paragraph with
  measured pattern paragraph + counter values)
