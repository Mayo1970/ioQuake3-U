# AGENTS.md — ioQuake3-U (Wii U native port)

> **Note for humans:** this file is written primarily for AI coding agents
> picking this project up cold. `README.md` is the end-user doc (build/install/
> controls); this one is the map of the codebase and the "why" behind its
> non-obvious decisions. `CLAUDE.md` also exists in this repo but has drifted
> behind several sessions of work — treat the code and this file as the source
> of truth over CLAUDE.md where they disagree.

## What this is

A **native Wii U** port of [ioQuake3](https://github.com/ioquake/ioq3) — Cafe OS,
`wut` toolchain, `.rpx`/`.wuhb` output. It is a **separate project** from the vWii
(libogc, `.dol`, Wii Virtual Console) port at `../../ioquake3-wii` — they share
only platform-agnostic engine/game code, never the system layer. Native mode
exists because vWii's 88 MB RAM ceiling and single 729 MHz core structurally
cap what's possible; native gets ~1 GB RAM and three 1.24 GHz Espresso cores.

There is **no dedicated-server build**. `Makefile.engine` (a `-DDEDICATED`
build) existed early on and was removed — it was never actually needed. The
`#ifndef DEDICATED` guards still scattered through `code/wiiu/*.c` and vendored
engine code are harmless (DEDICATED is never defined by `Makefile.client`); they
just haven't been stripped out.

## Build flavors

One Makefile (`Makefile.client`), flavor selected by target name or an `=1` var:

| Command | Flavor | Output | Defines |
|---|---|---|---|
| `make -f Makefile.client all` | ioQuake3-U (Q3A) | `ioquake3_wiiu.{rpx,wuhb}` | *(none)* |
| `make -f Makefile.client TA=1 ta` | Team Arena-U | `ioquake3_ta_wiiu.{rpx,wuhb}` | `-DSTANDALONETA -DMISSIONPACK` |
| `make -f Makefile.client OA=1 oa` | Open Arena-U | `ioquake3_oa_wiiu.{rpx,wuhb}` | `-DSTANDALONEOA` |
| `make -f Makefile.client CLASSIC=1 classic` | Quake3Classic-U | `ioquake3_classic_wiiu.{rpx,wuhb}` | `-DCLASSIC` |
| `make -f Makefile.client EF=1 ef` | Elite Force-U | `ioquake3_ef_wiiu.{rpx,wuhb}` | `-DELITEFORCE` |

**CLASSIC exists in the tree and builds, but is intentionally left out of
README.md for now.** It's a protocol-43 Dreamcast-crossplay flavor (ported from
the Wii/PS3/PS4 sibling ports' `#ifdef CLASSIC`/`ELITEFORCE` work,
`fixes/baseq3/zpack-classic.pk3` self-extracts on first boot). It connects and
plays on hardware, but has an **unresolved in-game performance regression**
(frame backend time climbs to 40-59 ms during play; a GX2 backface-cull fix was
tried and didn't fix it) that hasn't been root-caused. Don't re-document it
publicly until that's sorted; do feel free to keep fixing it.

**EF (Elite Force) is also left out of README.md — build-verified only (all 5
flavors compile clean from a forced clean rebuild), not yet hardware-tested.**
Ported from PS3's reference EF implementation across `msg.c`, protocol/wire
structs, trap-ABI dispatch (`g_public.h`/`cg_public.h`/`ui_public.h` +
dispatchers), botlib, and `renderergl1` struct diffs. Durable invariants:
- **Protocol 26 (own) vs. legacy 24 (retail wire) — never tie them together**
  like CLASSIC's proto-43-both. EF's `com_legacyprotocol == com_protocol` check
  is deliberately always-false for normal play; `clc.compat` only engages
  against a genuine retail EF server.
- **Trap-ABI ordinals are flavor-specific, not just widened.** EF's
  `gameImport_t`/`cgameImport_t`/`uiImport_t` are near-fully-parallel enums, not
  vanilla-plus-a-few-cases — cross-check every routed trap's ordinal *and* arg
  count against the enum in play, per flavor, not just against the dispatcher
  switch shape.
- **No fix-pak, no bots, v1.** `qagame` runs as retail EF's own bytecode
  `qagame.qvm` (this repo's native `code/game` — Q3A rules — is incompatible
  with EF's `playerState_t`/`entityState_t`/`usercmd_t` layout, so `GAME_SRCS`
  is empty for `EF=1` and `vm.c`'s native-qagame short-circuit excludes
  `ELITEFORCE`); botlib itself still answers bot syscalls from that bytecode
  qagame (it's engine-side), so `EA_AltAttack`/`EA_UseItem`/etc. exist, but no
  fix-pak or bot-behavior tuning has been done for v1.

Each flavor gets its own `build_client*/` object dir and its own icon
(`icons/{q3,ta,oa,qc,ef}.png`), so switching flavors never needs `make clean`.
`all-flavors` builds Q3/TA/OA/Classic/EF in sequence.

**Header changes require a forced clean rebuild to verify.** `Makefile.client`'s
object rule has no header-dependency tracking (no `-MMD`/`-MP`, no headers
listed as prerequisites) — editing a shared header and rebuilding can report
zero errors while silently skipping recompilation of `.c` files that only
changed via that header. `rm -rf build_client*` (or the specific flavor's
build dir) before trusting a "no errors" result after any header edit.

Build from **PowerShell**, not raw MSYS2 bash (breaks `gcc.exe` — machine-level
`TEMP=C:\Windows\TEMP`). Env block and exact commands are in `README.md`.

## Renderer architecture

There is no native OpenGL on Wii U. This port does **not** use ANGLE/EGL — that
path is dead: the full ANGLE client hung silently inside `eglInitialize` right
after loading `glslcompiler.rpl` under Cemu, and the ANGLE-era code
(`code/sys/`, `code/renderer/`, the legacy root `Makefile`'s `USE_ANGLE=1`
spike) has been deleted. Instead:

- **Frontend:** stock `code/renderergl1/` (vendored, effectively unmodified) —
  BSP walk, lighting, sorting, culling, all the normal ioq3 GL1 pipeline logic.
- **Backend:** `code/renderergx2/tr_gx2_backend.c` implements the GX2
  state/draw/texture calls the frontend expects, plus `GLimp_*`. This is the
  **Wii-sibling-port-proven architecture** (stock frontend + native backend),
  and replaced an earlier from-scratch custom frontend
  (`tr_gx2_stub/world/scene/model.c` — no longer compiled) that had no
  lightmaps, no entity rendering, and no translucency sorting.
- **Shaders:** one ubershader pair (`shaders/q3uber_{vert,frag}.glsl`,
  mode-selected via uniforms; alpha-test via shader `discard` since R700 has no
  fixed-function alpha test) is **precompiled offline** into a GX2 `.gsh`
  blob via CafeGLSL's host CLI (Linux/WSL-only tool, not part of this repo's
  build), then checked in as a C byte array in
  `code/renderergx2/tr_gx2_shader_gfd.c`. **There is no runtime shader
  compiler dependency (`glslcompiler.rpl`) at all** — that's deliberate: a
  runtime-compiled build used to hang on quit (console freeze requiring a power
  cycle) even after fixing the ANGLE-era boot hang. If you touch
  `shaders/q3uber_*.glsl`, you must regenerate `tr_gx2_shader_gfd.c` externally
  (WSL) and check in the new blob — editing the `.glsl` alone changes nothing
  on hardware.
- `qgl_gx2.c` is a no-op qgl function-pointer shim; the backend is called
  directly, not through GL entry points.

Known renderer gaps:
- **Mipmapping is forced off for all textures** (`numMips = 1` in
  `tr_gx2_image.c`'s image upload) after two separate corruption regressions —
  a placement/stride bug, then a `GX2CopySurface`-based tiled-texture mip
  generation attempt that also corrupted textures/shaders on hardware and was
  reverted on the spot (zero-tolerance policy on hardware regressions — see
  Hard rules below). Distant geometry aliases instead of blurring. Don't
  reattempt mip generation without a materially different approach; the two
  prior attempts are dead ends, not "almost working."
- Gamma correction (`r_gamma`) **does** work — it's the stock `renderergl1`
  software gamma bake at texture-upload time (`R_GammaCorrect`/
  `R_SetColorMappings` in `tr_image.c`), used because
  `glConfig.deviceSupportsGamma = qfalse` (no hardware gamma ramp on this
  platform). This isn't a gap; don't reintroduce a "missing gamma" fix.
- Lightmap/vertex-color shift must stay `<<2`, not `<<1` — there's no hardware
  gamma ramp to compensate the difference (see git history if this regresses;
  it looks like an off-by-one bit shift but isn't).
- Sampler state: world/model base textures need **wrap**, not the UI's
  **clamp** sampler. Sharing one sampler between the two was a real hardware
  bug (visible seams), not a style choice.

## Platform layer (`code/wiiu/`, `code/input/`, `code/audio/`)

| File | Role |
|---|---|
| `sys_main_wiiu.c` | Entry point: ProcUI loop, boot cmdline assembly per flavor, base path, Mii-nickname default name, exit handshake |
| `sys_wiiu.c` | Sys_\* layer: POSIX-ish paths/dirs over wut's newlib+SD devoptab; `Sys_LoadGameDll` statically links `qagame` instead of loading a DLL |
| `con_wiiu.c` | SD log (`log.txt`/`log_ta.txt`/`log_oa.txt`/`log_classic.txt`/`log_ef.txt`, per-line `fopen`/`fprintf`/`fclose` — durable across crashes) + `WHBLogUdp` |
| `net_wiiu.c` | Real UDP/IPv4 over `nsysnet` BSD sockets |
| `wiiu_account.cpp`/`.h` | `nn::act` Mii nickname lookup — C++-only API, read-only/non-blocking (unlike `nn::swkbd`, which hangs — see Hard rules) |
| `wiiu_pak_extract.c` | Extracts this flavor's embedded pak byte array to SD on first boot (q3 `baseq3/pak9-wiiu.pk3`, ta `missionpack/pak4-wiiu.pk3`, classic `baseq3/zpack-classic.pk3`; oa/ef have none). Source pk3 + destination picked by the `BUNDLED_PAK` block in `Makefile.client`, which generates `bundled_pak_embedded.h` |
| `code/input/wiiu_input.c` | VPAD (GamePad) + KPAD (Pro/Classic Controller) polling, stick filtering, DRC touch-as-mouse, rumble |
| `code/audio/wiiu_snd.c` | AX mixer backend, TV+DRC bus mix |

### ProcUI lifecycle — non-negotiable

Learned via silent OS kills on real hardware, not guesswork:
- Only `ProcUIInitEx` + callback registration are safe **before** the first
  `ProcUIProcessMessages(TRUE)` call. Never call `ProcUIProcessMessages(FALSE)`.
- The save callback must be non-`NULL` and must call
  `OSSavesDone_ReadyToRelease()`.
- Under HBL, call `OSEnableHomeButtonMenu(FALSE)` before `ProcUIInitEx` (HBL
  title-ID detection must run first).
- All heavy init (`CON_Init`, base path, pak preflight, `Com_Init`) is deferred
  to the **first** `PROCUI_STATUS_IN_FOREGROUND` tick inside the message loop —
  not before the loop starts.
- On exit: request foreground handoff (`SYSRelaunchTitle` if launched from HBL,
  else `SYSLaunchMenu`), keep pumping `ProcUIProcessMessages(TRUE)` until
  `PROCUI_STATUS_EXITING`, **then** `ProcUIShutdown()`, **then** GX2/WHBGfx
  teardown (`GLimp_ShutdownFinal`). Doing GX2 teardown before
  `ProcUIShutdown()` hangs in `EXITING` and needs a power cycle. This sequence
  is `WiiU_ExitHandshake()` in `sys_main_wiiu.c` — read it before touching
  shutdown order.
- Never touch GX2/WHBGfx present while backgrounded (revoked MEM1 = hardware
  crash). `CON_IsForeground()` gates this.

### Networking (`net_wiiu.c`) — hardware-validated, don't relitigate

- `recvfrom` length **must** be clamped to 1400 bytes
  (`WIIU_MAX_UDP_READ`) — IOSU fails **every** read with `EMSGSIZE` before even
  checking for data otherwise. This isn't a size optimization, it's a
  correctness requirement.
- `NET_Sleep` must **not** use `select()` — just a non-blocking `recvfrom`
  drain loop.
- A detached thread donates 3 MB via `somemopt(SOMEMOPT_REQUEST_INIT, ...)`
  before any socket call; the thread function blocks until `nsysnet` shuts
  down, so it's expected to look "stuck" in a debugger — that's by design.
- IPv4 only, one bound socket. Keep `NET_SelfTest` and the loud `Com_Printf`
  error paths — they're what made the EMSGSIZE bug diagnosable in the first
  place.

### Bot AI / VM architecture

`qagame` is **compiled natively and statically linked** (`Sys_LoadGameDll` in
`sys_wiiu.c` short-circuits to a direct `dllEntry`/`vmMain` call), not run as a
bytecode VM — interpreter overhead on `qagame` was the actual cause of
framerate drops with bots active, not the renderer. `cgame`/`ui` stay bytecode
(interpreted; **no JIT** — heap memory isn't executable on this platform, a PPC
JIT was tried and confirmed non-viable: the call hangs with no exception, and
the codegen area is gated/32 KB). Don't reattempt a QVM JIT here.

**EF is the one exception:** `GAME_SRCS` is empty for `EF=1` and `vm.c`'s
native-qagame short-circuit explicitly excludes `ELITEFORCE`, so `qagame` runs
as retail EF's own bytecode `qagame.qvm` there — this repo's native `code/game`
is vanilla Q3A rules, structurally incompatible with EF's
`playerState_t`/`entityState_t`/`usercmd_t` wire layout. Don't "fix" this by
trying to make EF use native qagame; it can't without reimplementing Raven's
actual EF game logic from scratch.

### Boot cmdline (assembled in `sys_main_wiiu.c`)

Base path is a fixed `fs:/vol/external01/quake3` (= `sd:/quake3/` under
Aroma/HBL) — no USB fallback, unlike the vWii sibling port. Per-flavor
`fs_game`/`vm_game`/log-suffix come from `WIIU_FS_GAME`/`WIIU_VM_GAME`/
`WIIU_LOG_SUFFIX` macros at the top of the file. Notable injected cvars:
`com_hunkMegs 256`, `com_zoneMegs 64`, `com_soundMegs 16`, `r_customwidth/height
1280/720`, `com_maxfps 60`, `j_pitch/yaw/forward/side` (small base values —
`cl_input.c`'s `__WIIU__` block multiplies stick look by `cl_sensitivity`, so
the in-game slider affects gamepad look speed too, not just mouse).
`UNZ_BUFSIZE=4194304` (Makefile define) cuts pk3-read IOSU round trips from the
stock 16 KB chunking; its read buffer is routed off the fixed zone onto the
general heap under `__WIIU__` so it doesn't risk zone exhaustion.

## Hard rules / dead ends (do not re-attempt without new evidence)

| Attempt | Why it failed | Prerequisite to retry |
|---|---|---|
| ANGLE (EGL + GLES2/3 → GX2) | Full client hung silently in `eglInitialize` after loading `glslcompiler.rpl` | None known — treat as dead, use the native GX2 backend |
| Runtime shader compilation (`glslcompiler.rpl`) | Console freeze on quit, even after the boot hang was worked around | Offline CafeGLSL precompile is the only shipped path; don't reintroduce a runtime compiler dependency |
| PPC QVM JIT | Heap memory not executable — call hangs, no exception; codegen area gated + 32 KB | None — bytecode interpreter is the only VM path for cgame/ui |
| `GX2CopySurface`-based mip generation | Regressed textures/shaders on hardware (root cause not diagnosed), reverted immediately per zero-tolerance-on-hardware-regressions policy | A materially different mip strategy — don't retry the same approach speculatively |
| Gyro motion controls (L3+R3 toggle) | Read path worked, but scaled feel was unsatisfactory after 3 hardware iteration cycles | Code was fully reverted — would need a fresh feel-tuning approach, not a bug fix |
| `nn::swkbd::Create()` on-screen keyboard | Hangs forever on real hardware despite compiling/linking clean; no known-good homebrew usage exists | New evidence that swkbd is usable from this environment at all |
| USB HID input | No SDK path exists on this platform | N/A |

## Debugging on hardware

**All testing happens on real Wii U hardware — Cemu is not used for validation
as of 2026-07-10.** No attached debugger; diagnose via SD logs and the crash
handler.

- `sd:/quake3/log{,_ta,_oa,_classic,_ef}.txt` — boot/runtime log, per-line
  `fclose`'d so it survives crashes and freezes.
- `sd:/quake3/crash{,_ta,_oa,_classic,_ef}.txt` — written by the DSI/ISI/PROGRAM
  exception handler (`wiiu_exception_handler` in `sys_main_wiiu.c`): PC, LR,
  DAR/DSISR, all GPRs, at the failing frame count.
- `sd:/quake3/error{,_ta,_oa,_classic,_ef}.txt` — written on `Sys_Error` /
  `Com_Error`.
- `WHBLogUdp` also mirrors console output for remote log watching.

## Where to look

| Need | File |
|---|---|
| End-user install/build/controls | [README.md](README.md) |
| Boot sequence, ProcUI, cmdline | [code/wiiu/sys_main_wiiu.c](code/wiiu/sys_main_wiiu.c) |
| Paths, `Sys_LoadGameDll` | [code/wiiu/sys_wiiu.c](code/wiiu/sys_wiiu.c) |
| GX2 backend (hot path) | [code/renderergx2/tr_gx2_backend.c](code/renderergx2/tr_gx2_backend.c) |
| Shader precompile pipeline | [code/renderergx2/tr_gx2_shader.c](code/renderergx2/tr_gx2_shader.c), `tr_gx2_shader_gfd.c` |
| Input (VPAD/KPAD/touch/rumble) | [code/input/wiiu_input.c](code/input/wiiu_input.c) |
| Networking | [code/wiiu/net_wiiu.c](code/wiiu/net_wiiu.c) |
| Build logic / flavors | [Makefile.client](Makefile.client) |
| Phased history / renderer analysis | [docs/PORTING-PLAN.md](docs/PORTING-PLAN.md) *(historical — describes an earlier custom-frontend architecture since superseded; the tables above reflect current reality)* |
