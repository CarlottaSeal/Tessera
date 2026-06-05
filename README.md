# Vulkan Tile-Deferred Renderer

A Vulkan demo built on top of an in-house C++ engine to exercise the tile-friendly
deferred rendering pattern that mobile GPUs (Adreno, Mali, PowerVR) reward with
significant bandwidth savings. Runs the deferred path and a forward A/B reference
side by side so the bandwidth payoff can be measured directly.

![scene](Run/Data/Images/Show.png)

## Where the code lives

This repository is the **demo app** — game-side code, shaders, docs, and the
`Run/` folder. The actual Vulkan rendering implementation is in a separate
multi-API C++ engine that backs DX12, DX11, and Vulkan from a shared
abstraction. That engine is a much larger project re-used across several
games (RedCraft, Chess Soul, LuminaGI's GI subsystem, this demo); it lives in
its own repository.

The Vulkan-relevant engine files (paths shown relative to the engine repo):

| File | Lines | What it does |
| --- | --- | --- |
| `Engine/Code/Engine/Renderer/VulkanRenderer.{h,cpp}` | ~3880 | Instance/device/swapchain/queues, bindless atlas (256-slot `sampler2D[]` with `descriptorBindingPartiallyBound`), 4 MB model UBO ring (dynamic offset), camera UBO ring, descriptor set layouts, pipeline cache, draw redirect for MT secondary recording, `VkMaterialPC` 16 B push constant carrying `{DiffuseId, NormalId, SpecularId, _pad}` for DX12 parity. |
| `Engine/Code/Engine/Renderer/VulkanDeferredPath.{h,cpp}` | ~1100 | The deferred render pass + lighting subpass + forward A/B render pass + forward overlay render pass + per-thread secondary cmd buffer + pool double-buffering, GPU timestamp pool with per-slot mode tracking, `LAZILY_ALLOCATED` G-buffer alloc with `DEVICE_LOCAL` fallback + startup log, CPU-side recording-time `std::chrono` ring + 64-frame rolling avg. |
| `Engine/Code/Engine/Renderer/VulkanMemoryPool.{h,cpp}` | smaller | Pool allocator for Vulkan device memory (per memory type, sub-allocation from large blocks). |

What's in **this** repo:
- `Code/Game/` — game-side code (App.cpp, Game.cpp, Player.cpp, Prop.cpp, Entity.cpp); F2/F3/F4 toggles; piece spawn + scene setup
- `Run/Data/Shaders/Vulkan/` — GLSL sources (`gbuffer.vert/frag`, `gbuffer_pcutbn.vert/frag`, `lighting.frag`, `forward_lit.frag`, `forward_lit_pcutbn.frag`, `default.vert/frag`) and their `.spv` builds
- `Run/Data/Models/LewisSet/` — chess piece GLBs + auto-extracted normal maps
- `docs/` — talking points, perf baseline captures, deployment checklist
- `Run/VulkanTest_Release_x64.exe` — runnable build

If you want to see the actual Vulkan plumbing (subpass setup, bindless
descriptor binding, MT secondary cmd recording, lazy-alloc fallback,
timestamp double-buffering), it's in those engine files; happy to walk
through them on a screen-share.

## What it does

- One `VkRenderPass` with two subpasses: G-buffer fill → lighting via `subpassInput`.
- G-buffer attachments are created with `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT` and
  prefer `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`. On a tiler the driver keeps them
  in tile memory for the whole pass; the only attachment that actually goes to DRAM
  is the swap image written by the lighting subpass.
- Subpass dependency between G-buffer and lighting carries `VK_DEPENDENCY_BY_REGION_BIT`
  — the keyword that lets the driver hand the tile off subpass-to-subpass without
  flushing.
- Forward A/B path: a separate single-pass equivalent that evaluates the same
  Blinn-Phong lighting per fragment with no G-buffer. F2 toggles between the two at
  runtime; the HUD shows GPU time for both so the cost difference is immediate.
- Forward overlay render pass with `LOAD_OP_LOAD` on the swap image for HUD,
  dev-console, and screen-space debug text.
- Per-frame GPU timestamps for both paths, double-buffered across
  `MAX_FRAMES_IN_FLIGHT` so readback never blocks on in-flight work.

## Why it's interesting on mobile

On a desktop IMR (NVIDIA, AMD), splitting deferred shading across two render passes
is fine — the G-buffer just sits in VRAM and the lighting pass reads it back. The
write-then-read costs some bandwidth but L2 absorbs most of it.

On a tile-based renderer (Adreno, Mali, PowerVR), two-pass deferred is expensive:
the rasterizer flushes the G-buffer out of tile memory to DRAM at the end of pass 1,
then reloads it tile-by-tile at the start of pass 2. For a 1080p G-buffer
(R8G8B8A8 albedo + A2B10G10R10 normal + D32 depth ≈ 12 bytes/pixel × 2M pixels
× 2 directions) that's ~48 MB of avoidable bandwidth per frame.

Putting both subpasses inside the same render pass with `BY_REGION_BIT` lets the
driver keep the G-buffer tile-local. `subpassLoad()` in the lighting fragment shader
is the corresponding intrinsic — it samples only the current pixel of the input
attachment, which is exactly what tilers can satisfy from on-chip SRAM.

## Render pass layout

```
deferred render pass — 4 attachments, 2 subpasses
  attachment 0: gAlbedo  R8G8B8A8_UNORM        rgb=albedo  a=shininess/256 (8-bit) TRANSIENT, LAZILY_ALLOCATED preferred
  attachment 1: gNormal  A2B10G10R10_UNORM     rgb=normal (10b/ch)  a=unused (2b)  TRANSIENT
  attachment 2: gDepth   D32_SFLOAT            depth                               TRANSIENT
  attachment 3: swap     swapchain format      finalLayout = COLOR_ATTACHMENT_OPTIMAL

  subpass 0 (G-buffer): writes 0,1 + depth 2
  subpass 1 (lighting): reads 0,1,2 as input attachments, writes 3
  dep 0→1: COLOR_ATTACHMENT_OUTPUT | LATE_FRAGMENT_TESTS  →  FRAGMENT_SHADER
           COLOR_ATTACHMENT_WRITE  | DEPTH_STENCIL_WRITE  →  INPUT_ATTACHMENT_READ
           VK_DEPENDENCY_BY_REGION_BIT

forward render pass — 2 attachments, 1 subpass (used when F2-toggled into A/B mode)
  attachment 0: swap     LOAD_OP_CLEAR         finalLayout = COLOR_ATTACHMENT_OPTIMAL
  attachment 1: depth    D32_SFLOAT            LOAD_OP_CLEAR
  subpass 0: PCU/PCUTBN draws sample 6 lights inline per fragment

overlay render pass — 1 attachment, 1 subpass
  attachment 0: swap     LOAD_OP_LOAD          finalLayout = PRESENT_SRC_KHR
  subpass 0: PCU forward draws (HUD, dev console, screen-space debug)
```

Each frame either runs deferred → overlay (default) or forward → overlay
(F2-toggled). The HUD shows the most recent timing for each path so a single
session's left-half-vs-right-half comparison is straightforward.

## Pipelines

| pipeline                  | render pass / subpass     | vertex layout    | shaders                                                  |
|---------------------------|---------------------------|------------------|----------------------------------------------------------|
| `m_gbufferPipeline`       | deferred subpass 0        | `Vertex_PCU`     | `deferred/gbuffer.vert + gbuffer.frag`                   |
| `m_gbufferPipelinePCUTBN` | deferred subpass 0        | `Vertex_PCUTBN`  | `deferred/gbuffer_pcutbn.vert + gbuffer.frag`            |
| `m_lightingPipeline`      | deferred subpass 1        | none (full-screen tri from `gl_VertexIndex`) | `deferred/lighting.vert + lighting.frag` |
| `m_forwardPipeline`       | forward subpass 0         | `Vertex_PCU`     | `deferred/gbuffer.vert + forward_lit.frag`               |
| `m_forwardPipelinePCUTBN` | forward subpass 0         | `Vertex_PCUTBN`  | `deferred/gbuffer_pcutbn.vert + forward_lit.frag`        |
| `m_overlayPipeline`       | overlay subpass 0         | `Vertex_PCU`     | `default.vert + default.frag` (alpha blend, no depth)    |

The G-buffer PCU and PCUTBN pipelines share a pipeline layout and fragment shader;
only the vertex stage and vertex input description differ. The engine's
`DrawIndexBuffer` picks between the two by checking the bound VBO's stride. The
forward pipelines reuse the same vertex shaders and a different fragment shader
that does the lighting evaluation inline.

`SetPipelineOverride(VkPipeline pcu, VkPipeline pcutbn, VkPipelineLayout layout)`
is the engine hook the deferred path uses to inject its pipelines without changing
the engine's draw call sites. The optional `layout` override is needed for the
forward path: its pipeline declares an extra descriptor set (lights UBO at set 1)
that the engine's main layout doesn't, so binding descriptor set 0 with the engine's
layout would invalidate set 1. Passing the override layout keeps both bound
correctly.

## Scene

LewisSet chess pieces — full back row (rook, knight, bishop, queen, king, bishop,
knight, rook) plus the 8-pawn line, 16 pieces total — lit by 6 animated point
lights orbiting the formation. Pieces are real GLB meshes loaded through the
engine's `StaticMesh` (PCUTBN vertex layout). Each piece's diffuse texture is
extracted from the GLB at load time; tangent and bitangent attributes feed the
PCUTBN G-buffer pipeline. Light positions are visualised via `DebugAddWorldPoint`
and rendered through the deferred path so they sit at correct depth.

## Engine changes that landed alongside this

The engine's Vulkan path was already partially built but had a handful of bugs that
made it unusable for anything beyond clear-color demos. Fixing them was a
prerequisite for the deferred path to be testable:

- UBO ring buffer for camera (set 0 binding 0) and model (set 0 binding 4) using
  `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` + dynamic offsets, sized for several
  thousand draws per frame.
- VBO ring buffer (`AppendDataVulkan`) so `DrawVertexArray` doesn't allocate per
  call.
- Bindless texture atlas (256 combined image samplers at set 0 binding 1) +
  4-byte push constant carrying the atlas slot index per draw.
  `RegisterTextureBindless` lazily writes a default white texture into all 256
  slots on first call so unbound slots don't trip validation.
- `EndCamera` no longer ends the active render pass — letting the deferred path
  own the pass across multiple `BeginCamera` / `EndCamera` calls (debug-render
  world, light gizmos, etc.) without spurious clears.
- V-flip happens in the vertex shader (negative-height viewport), keeping math in
  the standard Y-up convention while honouring Vulkan's Y-down clip space.
- `DebugRenderSystem` ifdefs widened from `ENGINE_DX11_RENDERER` to
  `ENGINE_DX11_RENDERER || ENGINE_VULKAN_RENDERER` so the Vulkan path can use the
  same debug primitives the DX paths do.
- Pipeline-layout override on `SetPipelineOverride` so injected pipelines that
  declare additional descriptor sets don't invalidate bindings made under the
  engine's main layout.

After these fixes the Vulkan path matches the DX12 path's draw-call surface area
and is the foundation the deferred path is built on.

## Lighting

Per-pixel Blinn-Phong with radial-falloff attenuation, evaluated either:
- in the lighting subpass (deferred mode) by sampling the G-buffer via
  `subpassLoad()` and reconstructing world-space position from depth, or
- in the geometry fragment shader (forward A/B mode) directly from interpolated
  world position and normal.

The G-buffer packs the Phong exponent into `gAlbedo.a`, encoded as `shininess / 256`
in the 8-bit unorm slot. `gNormal.a` is left unused — it's only 2 bits in
`A2B10G10R10_UNORM_PACK32`, far too coarse for material data, so storing anything
there causes severe quantization (was the source of an early "deferred is too
bright" bug — 2-bit shininess of `0.125` rounded to `0`, making
`pow(NdotH, 0) = 1` and turning the specular term into a full-hemisphere additive
contribution). Specular intensity is a per-frame constant matching the forward
path; per-material spec intensity would need a separate channel or a material
index lookup.

## Timestamp queries

Ten timestamp queries in one pool, divided into two 5-slot blocks — one per
in-flight frame. Slot `f` writes queries `[5f, 5f+4]`:

```
slot 0: [0] top-of-pipe  [1] end-gbuffer-subpass  [2] end-lighting-subpass
        [3] forward-pass-start  [4] forward-pass-end
slot 1: [5] ...                                    [9] ...
```

Each frame slot also tracks which mode (deferred or forward) was last recorded
into it, so the previous-frame readback knows which queries to read. At the start
of each frame's `BeginGBuffer` or `BeginForwardLit`, before the slot is reset
and rewritten, the slot's previous values are read out — the engine's
`vkWaitForFences` at frame start guarantees the slot's last submission is
GPU-complete, so the read is non-blocking and never trips the validation layer's
"query not reset" check. The cached values are exposed via `TryGetLastFrameTimings`
(deferred) and `TryGetLastForwardMs` (forward) and shown in the HUD.

## Multi-threaded subpass-0 recording

Vulkan's flagship architectural advantage — recording command buffers in
parallel and merging via `vkCmdExecuteCommands` — is wired up here. F3 toggles
it on/off at runtime; the HUD shows `MT: ON / OFF` next to the deferred/forward
mode label, so frame timings can be A/B compared in the same session.

**Layout: 1 main thread + 2 worker threads, 3 secondaries per frame slot.**

```
deferred subpass 0 contents = SECONDARY_COMMAND_BUFFERS  (when MT on)

main thread (secondary[0]):                    worker 1 (secondary[1]):       worker 2 (secondary[2]):
  cubes / sphere / grid via engine API           pieces 0..7 via raw vkCmd*     pieces 8..15 via raw vkCmd*
  light gizmos via DebugRenderWorld              (PreparedDraw replay)          (PreparedDraw replay)
  (recorded INTO secondary[0], not primary,
   via SetActiveRecordingTarget redirect)

main thread joins workers, then
  vkCmdExecuteCommands(primary, 3, secondaries)
  vkCmdNextSubpass(primary, INLINE) for lighting
  vkCmdEndRenderPass(primary)
```

**Prepare-then-record split.** The engine's `DrawIndexBuffer` mixes three
thread-unsafe operations into the per-draw critical path: model UBO ring
writes (host-mapped, shared offset), bindless texture registration
(`vkUpdateDescriptorSets` on a shared set), and dynamic descriptor binds. So
workers can't call the engine API directly. Instead, main captures all per-piece
state into a `PreparedDraw` struct ahead of the worker kick:

```cpp
struct PreparedDraw {
    VkPipeline    pipeline;
    VkBuffer      vbo, ibo;
    uint32_t      indexCount;
    uint32_t      cameraDynamicOffset;   // captured from VulkanRenderer state
    uint32_t      modelDynamicOffset;
    VkMaterialPC  material;              // 16-byte push constant snapshot
};
```

Workers then issue raw `vkCmd*` calls into their own secondary cmd buffers —
no engine API touched, no shared writable state read.

**Per-thread command pools, double-buffered.** Each thread owns
`VkCommandPool[MAX_FRAMES_IN_FLIGHT]`; only the slot whose previous use has
GPU-completed (guaranteed by the engine's frame-start fence wait) is reset and
reallocated. Pool reset is per-thread — no cross-thread sync — and Vulkan's
"command pool is externally synchronised" rule is satisfied because each pool
is only ever touched by one thread.

**Parallelism overlap.** The kick is non-blocking: main fires the two workers
via `std::async` and immediately continues recording its own debug-render
geometry into `secondary[0]`. Only then does it `wait()` on the workers, so
main and workers genuinely overlap.

**Honest measurement note (measured 2026-05-05).** Tested MT off vs on at
piece counts of 16, 256, and 1024 via the F4 stress mode. Across all three,
the cpu-rec timer showed **no observable delta** between MT off and MT on.
The architectural pattern is shipped and toggleable; the measured benefit
on this hardware at this scale is zero.

Diagnosis: the MT split parallelizes only the record phase — workers
replaying `vkCmd*` from captured `PreparedDraw` structs. The prepare phase
runs on main and scales linearly with piece count: every piece calls
`SetModelConstants` (advances the model UBO ring) and `SetMaterialConstants`
(writes the bindless slot trio). Both touch shared mutable engine state and
can't be parallelized without an engine-side refactor (e.g., lock-free
thread-local UBO pools per worker). Record-phase cost is small relative to
prepare-phase at these draw counts, so even fully parallelizing record
buys little.

To make MT measurably help in this implementation, the prepare phase
itself would need restructuring. Full per-piece-count breakdown plus
diagnosis is in [`docs/perf_baseline.md`](docs/perf_baseline.md) Test D.

### Normal mapping

PCUTBN tangent / bitangent attributes are plumbed through `gbuffer_pcutbn.vert`,
and each chess piece's normal map is auto-extracted from its GLB at load time
(`Models/LewisSet/<piece>_normal.png` generated by `StaticMesh`). The
PCUTBN-only fragment shaders (`gbuffer_pcutbn.frag` for deferred,
`forward_lit_pcutbn.frag` for forward A/B) sample the normal map at
`mat.NormalId`, decode `[0,1] → [-1,1]`, construct `mat3(T, B, N)` from the
interpolated tangent / bitangent / normal, and transform the tangent-space
normal to world space before writing to `gNormal.rgb`. Diffuse / normal /
specular bindless slot ids ride together in a 16-byte `MaterialConstants` push
constant — same layout DX12's `MaterialConstants` CB uses, so `BindTexture(t,
slot)` and `SetMaterialConstants(d, n, s)` work identically across the two
backends.

## Build

Solution: `VulkanTest.sln`. Tested with VS 2022 + Windows SDK 10.0.26100 + Vulkan
SDK 1.4.335.0. The project references `$(VULKAN_SDK)/Include` and
`$(VULKAN_SDK)/Lib` on `Debug|x64` and `Release|x64`; only those two
configurations are actively maintained.

The `Engine` project builds with `ENGINE_VULKAN_RENDERER` defined;
`ENGINE_DX12_RENDERER` remains available for the DX12 build but isn't used here.

Shader compilation is manual — `.vert` / `.frag` files under
`Run/Data/Shaders/Vulkan/` are compiled to `.spv` with `glslangValidator` (from
the Vulkan SDK). The deferred path expects:

```
Run/Data/Shaders/Vulkan/default.vert.spv
Run/Data/Shaders/Vulkan/default.frag.spv
Run/Data/Shaders/Vulkan/deferred/gbuffer.vert.spv
Run/Data/Shaders/Vulkan/deferred/gbuffer_pcutbn.vert.spv
Run/Data/Shaders/Vulkan/deferred/gbuffer.frag.spv
Run/Data/Shaders/Vulkan/deferred/lighting.vert.spv
Run/Data/Shaders/Vulkan/deferred/lighting.frag.spv
Run/Data/Shaders/Vulkan/deferred/forward_lit.frag.spv
```

## Controls

- `Space` / `N` — leave attract mode and enter the 3D scene
- WASD + mouse — fly camera (xy translate + look)
- `Z` / `C` — descend / ascend
- `Q` / `E` — roll
- `Shift` — speed boost
- `H` — recenter camera
- `F2` — toggle deferred ↔ forward render path (HUD shows current mode + GPU ms)
- `F3` — toggle multithreaded subpass-0 recording on/off
- `~` (tilde) — toggle dev console
- `Esc` — back to attract mode, then quit

## Files of interest

```
Engine/Code/Engine/Renderer/VulkanDeferredPath.{h,cpp}    deferred + forward + overlay passes
Engine/Code/Engine/Renderer/VulkanRenderer.{h,cpp}        engine Vulkan backend
VulkanTest/Run/Data/Shaders/Vulkan/deferred/*.{vert,frag} pipeline shaders
VulkanTest/Code/Game/Game.{hpp,cpp}                       scene + per-frame render flow
```

## Adreno deployment checklist

To turn this into a real "I measured X% bandwidth savings on Adreno" interview
data point, the demo needs to deploy to an actual tile-based GPU. Notes for
running it down end-to-end:

1. **NDK Vulkan port.** The renderer's surface creation uses Win32 (`HWND` /
   `vkCreateWin32SurfaceKHR`); replace with `ANativeWindow` /
   `vkCreateAndroidSurfaceKHR`. Replace `WinMain` with the Android
   `android_main` entry point (`android_native_app_glue`) and route input
   events accordingly.
2. **Vulkan loader / validation on Android.** `libvulkan.so` ships with the
   device. Validation layers must be loaded as a separate APK
   (`VK_LAYER_KHRONOS_validation`) and the loader env vars set up via the
   debug layer manifest. Don't ship debug builds with validation — the perf
   numbers stop being meaningful.
3. **Asset packaging.** Models, textures, and SPIR-V need to land in the APK's
   assets directory; replace direct file I/O paths with `AAssetManager`
   reads.
4. **Memory-type selection.** The engine's
   `FindMemoryTypeWithFallback(LAZILY_ALLOCATED, DEVICE_LOCAL)` already prefers
   lazily-allocated memory. On Adreno this resolves to genuinely tile-local
   storage; on a desktop NVIDIA card it falls back to DEVICE_LOCAL VRAM. No
   code change needed — verify with
   `vkGetPhysicalDeviceMemoryProperties` at startup that the type with
   `LAZILY_ALLOCATED_BIT` is being chosen.
5. **Bandwidth measurement.** Two main routes:
   - **Streamline / Snapdragon Profiler.** Captures GPU counters including
     "Bytes read from system memory" and "Bytes written to system memory".
     Compare those across the F2-toggled deferred / forward modes for a clean
     bandwidth diff.
   - **Vendor-internal counters via `VK_KHR_performance_query`** if the device
     exposes them. Less portable, more direct.
6. **Sanity counters.** Beyond bandwidth, check tile count, primitives per tile,
   and percentage of fragments killed by early-Z. Forward mode should show a
   measurably higher bytes-read counter (G-buffer detile + retile) than
   deferred.
7. **Thermal stability.** Adreno throttles aggressively. Run each test for 60+
   seconds, throw away the first 30 seconds, average over the next 30 to
   compare apples to apples.

The deferred-vs-forward A/B path is already wired up so once the demo runs on
device, swapping F2 mid-capture gives back-to-back perf-counter snapshots that
attribute the bandwidth difference cleanly to the render-path choice.

## Caveats

- Only tested on a desktop NVIDIA card (RTX-class). The F2 toggle between
  deferred and forward shows essentially identical frame times here, which is
  the expected result on a desktop IMR: the full G-buffer (~50 MB at 1080p) fits
  comfortably in the GPU's L2 (~96 MB on Ada-class parts), so the two-pass
  forward path's "extra" G-buffer read/write traffic is absorbed by cache and
  never hits DRAM. The bandwidth payoff this design is built around is
  tile-based-GPU-specific — Adreno / Mali tile memory is one to two orders of
  magnitude smaller than desktop L2, the G-buffer doesn't fit, and a two-pass
  design pays a per-tile detile + retile to DRAM that single-pass + input
  attachments avoids. See the deployment checklist for getting real numbers
  off Snapdragon Profiler.

## Measuring on desktop with Nsight Graphics

Captures and the full counter table are in
[`docs/perf_baseline.md`](docs/perf_baseline.md). Headline measured numbers
on RTX 4080 Laptop (NVIDIA Vulkan driver, Nsight Graphics 2025.3):

- **L2 Section Hit Rate, lighting subpass: 88.5 %** — confirms the
  G-buffer fits in L2 on this hardware. The lighting subpass is the single
  3-vertex `vkCmdDraw` after the `vkCmdNextSubpass`; that's the window
  where `subpassLoad` reads the G-buffer attachments, and ~89 % of those
  reads hit L2.
- **VRAM Throughput, lighting subpass: 5.6 % of peak** — almost no VRAM
  traffic during lighting. Consistent with the G-buffer being L2-resident.
- **L2 Throughput, lighting subpass: 12.4 % of peak** — moderate L2 use
  during the full-screen triangle, expected since the work is mostly
  fragment shading, not memory.
- **`LAZILY_ALLOCATED_BIT` memory type — not exposed by NVIDIA desktop
  Vulkan driver**, so the fallback to `DEVICE_LOCAL_BIT` engages and the
  G-buffer lands in standard device-local memory. This is the expected,
  correct outcome on this hardware; the same code path picks a real lazy
  heap on Adreno where the type does exist. Receipt: `[gbuffer]` startup
  log in `Run/gbuffer_alloc.log` shows `lazy=NO` for all three attachments.
- **Frame time delta between deferred and forward: not significant** —
  on a desktop IMR, both paths fit comfortably in L2 and frame time stays
  ~equal. The bandwidth differential the design targets is mobile-specific.

Workflow if you want to reproduce or extend:

1. Install [Nsight Graphics](https://developer.nvidia.com/nsight-graphics) (free).
2. Run `Run/VulkanTest_Release_x64.exe`, enter the 3D scene, idle the camera.
3. In Nsight: `Connect → Local`, point at the exe, Activity: **GPU Trace**.
4. Press Generate to capture a frame; in the report timeline, select the
   3-vertex draw inside subpass 1 (the lighting pass) — right panel
   aggregates the counters over that selection.
5. F2 toggles between deferred and forward; F3 toggles MT recording;
   F4 cycles piece count for stress testing.

In-app, the HUD already shows GPU timestamps for both modes
(`gbuf X ms  lighting Y ms` for deferred, `forward total Z ms` for forward),
double-buffered so the previous frame's timing is read non-blocking. On
desktop these are basically equal; on Adreno the deferred total should sit
noticeably below forward total once the G-buffer overflows tile memory.
- Lighting is point-light only (max 8, demo uses 6), no shadows. The G-buffer
  packs diffuse colour + specular intensity in attachment 0 and world normal +
  shininess exponent in attachment 1; the lighting subpass evaluates a
  Blinn-Phong loop per pixel with radial-falloff attenuation.
- `subpassLoad` reconstructs world-space position from depth in the lighting
  fragment shader. That works for this demo's matrix conventions but isn't
  optimal — a tiler-friendly alternative would be to pack view-space position
  directly into a third G-buffer attachment.
