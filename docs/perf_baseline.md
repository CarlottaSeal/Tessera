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

| Counter                    | Value     |
| -------------------------- | --------- |
| DRAM Read Throughput       | _GB/s_    |
| DRAM Write Throughput      | _GB/s_    |
| L2 Hit Rate                | _%_       |
| L2 Throughput              | _GB/s_    |
| HUD: gbuf ms               | _ms_      |
| HUD: lighting ms           | _ms_      |
| Frame time (Nsight)        | _ms_      |

## Test B — FORWARD, MT off (A/B partner for A)

Press F2 once. HUD must read `mode: FORWARD`.

| Counter                    | Value     |
| -------------------------- | --------- |
| DRAM Read Throughput       | _GB/s_    |
| DRAM Write Throughput      | _GB/s_    |
| L2 Hit Rate                | _%_       |
| L2 Throughput              | _GB/s_    |
| HUD: forward total ms      | _ms_      |
| Frame time (Nsight)        | _ms_      |

**Expected delta (desktop IMR):** small. README claim is "single-digit MB per
frame ~ 1-3% of total bandwidth". Confirm direction (deferred lower DRAM
read), magnitude, and that frame time is essentially equal — that *is* the
honest story for desktop. The size-up to Adreno tile memory is the talking
point; this is the proof the design isn't *worse* on desktop.

## Test C — DEFERRED, MT ON vs MT OFF

Two captures, F3 toggle between them. Same scene as A.

| Counter                       | MT OFF    | MT ON     |
| ----------------------------- | --------- | --------- |
| Frame time (Nsight)           | _ms_      | _ms_      |
| HUD: gbuf ms                  | _ms_      | _ms_      |
| CPU subpass-0 record time*    | _µs_      | _µs_      |

*Need a CPU-side timer around `BeginGBuffer → EndGBufferAndRunLighting`
recording phase. Add `std::chrono::steady_clock` bracket if not already there.

**Expected:** identical frame time at 16 pieces (~30 draws). This is the
honest "architecture proven, not benefit" point — back it with the actual
numbers, don't hand-wave.

## Test D — Stress scene (D-3, after C is in)

Multiply piece count via grid layout in `Game.cpp` (TBD: 8×8 = 256, then
16×16 = 1024). Same A/B as C.

| Pieces | MT OFF frame ms | MT ON frame ms | MT OFF CPU rec µs | MT ON CPU rec µs |
| ------ | --------------- | -------------- | ----------------- | ---------------- |
| 16     |                 |                |                   |                  |
| 256    |                 |                |                   |                  |
| 1024   |                 |                |                   |                  |

**Expected:** crossover point where MT ON CPU recording time visibly
diverges. GPU time will also climb (more geometry) — separate the two so
the story is "CPU recording goes from X to Y on main thread; with 3 threads
it stays at Z" not "FPS got better".

## Test E — LAZILY_ALLOCATED verification

Not Nsight-driven; runtime introspection.

- Walk `vkGetPhysicalDeviceMemoryProperties` heap list; record which heaps
  have `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`.
- For each G-buffer attachment image: `vkGetImageMemoryRequirements` →
  `memoryTypeBits` → confirm a lazy heap is in the candidate set, and
  confirm the chosen `vkAllocateMemory` actually picked it.
- Add a one-shot startup log line: `[gbuffer] image N allocated on heap H,
  lazy=YES/NO`.

**Why this matters:** the entire transient-attachment story rests on the
allocation actually being lazy. If the driver fell back to a normal heap,
the talking-point is wrong. Log line is the receipt.

---

## Numbers → talking points cross-reference

When filled, paste specific values into:

- TALKING_POINTS.md §2-min point 1 (replace "single-digit MB" with actual
  delta from Test A vs B)
- TALKING_POINTS.md §5-min point 8 (replace "microseconds per frame" with
  actual µs from Test C, and Test D crossover point)
- README §"Measuring on desktop" (replace expected-pattern paragraph with
  measured pattern paragraph + counter values)
