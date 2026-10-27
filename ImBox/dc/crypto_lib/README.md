# DCrypt `crypto_lib`

The cryptographic core of DiskCryptor, under the **MIT license**, with no GPL or LGPL code
in it.

This replaces `crypto/crypto_fast`. Same public API, so consumers only change an include
path. 32-bit x86 and VIA PadLock are gone; amd64 and ARM64 remain.

## Licensing

Everything here is MIT except the vendored third-party pieces listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md), which are as permissive as MIT or more so
(BSD-style, public domain, CC0). Per-file `SPDX-License-Identifier` tags are authoritative.

One obligation survives into binary releases: Gladman's AES assembler requires its notice to
be reproduced in the documentation of binary distributions, and forbids using his name to
endorse products. Shipping `THIRD-PARTY-NOTICES.md` with the product covers it. Nothing else
in the tree constrains a binary release at all.

Licensing this directory MIT does not change anything else in the repository. `driver/`,
`gui/`, `boot/`, `dcapi/` and `setup/` remain GPL-3.0, and so does DiskCryptor as a shipped
product — MIT code combines into a GPL work without friction. What changes is that this
directory can be reused on its own, by projects under any license.

The three genuinely GPL third-party files in the old tree — the Linux kernel's Serpent and
Twofish, and Joachim Fritschi's Twofish assembler — were replaced by implementations written
from the algorithms' published specifications. Everything ntldr wrote (GPL-3.0-only) was
likewise rewritten from the relevant standards: IEEE 1619 for XTS, RFC 2104, RFC 8018,
NIST SP 800-90A, FIPS-197.

## Layout

```
serpent.c/.h              Serpent-256, scalar. Key schedule + single block.
serpent_sboxes.h          The eight S-boxes and the linear transform, once, as
                          type-agnostic macros. Every core includes this.
serpent_simd.inl          The 32-round schedule on top of those S-boxes.
twofish.c/.h              Twofish-256, full-table.
aes_key.c/.h              AES-256 key schedules (both directions).
aes_asm.h                 Picks the AES block core for the target.
amd64/aes_amd64.nasm      Gladman's AES assembler core (permissive; unmodified).
sha512.c/.h               SHA-512 (LibTomCrypt, public domain).
sha512_hmac.c/.h          HMAC-SHA-512.
sha512_pkcs5_2.c/.h       PBKDF2-HMAC-SHA-512.
sha512_hmac_drbg.c/.h     HMAC_DRBG.
crc32.c/.h                CRC-32 and CRC combination.
xts_fast.c/.h             XTS mode, cascades, and runtime core selection.
xts_twofish.c/.h          XTS-Twofish, 4 blocks interleaved (portable)
xts_aes_ni.c/.h           XTS-AES on AES-NI            (amd64)
xts_serpent_x86.inl       The 128-bit Serpent core, compiled twice:
xts_serpent_sse2.c/.h       as SSE2                    (amd64)
xts_serpent_avx.c/.h        as VEX-128 /arch:AVX       (amd64, user mode only)
xts_serpent_avx2.c/.h     XTS-Serpent on AVX2, 8-wide  (amd64, user mode only)
xts_aes_ce.c/.h           XTS-AES on crypto extensions (ARM64)
xts_serpent_neon.c/.h     XTS-Serpent on NEON, 4-wide  (ARM64)
cl_platform.h             Kernel/user differences (AVX state, mainly).
cl_cpu.c/.h               CPU feature probes. Built with no /arch switch, on
                          purpose - see "Argon2 and AVX2".
cl_alloc.h                Allocator hook, for Argon2. Declares the two host
                          functions itself rather than including a consumer's header.
Argon2/                   Argon2id (CC0).
tools/                    Generators for the constant tables. Not built.
tests/                    Benchmark and differential tests against crypto_fast,
                          plus a runner for the crypto/tests vector corpus.
```

Serpent has five builds — scalar, SSE2, VEX-128, AVX2, NEON — and they share one copy of the
S-box definitions in `serpent_sboxes.h` and one copy of the round schedule in
`serpent_simd.inl`, differing only in their vector primitives. SSE2 and VEX-128 go further
and share the whole XTS body in `xts_serpent_x86.inl`, differing only in a compiler flag.
They cannot drift apart.

## Projects

| Project | Toolset | Platforms |
|---|---|---|
| `crypto_lib.vcxproj` | v143 | x64, ARM64 |
| `crypto_lib_km.vcxproj` | WindowsKernelModeDriver10.0 | x64, ARM64 |

Both produce a static library into `Bin\$(Configuration)_amd64\` or `..._arm64\`.
`amd64/aes_amd64.nasm` is assembled by `tools\yasm\yasm.exe` from the solution root.

### EFI

There is no third project file. EDK2 keeps a module's sources inside the module
directory and compiles them itself, so `DcsPkg/Library/DiskCryptorLib` reaches the
library through a junction (`ci_build_dcs.cmd` creates it) and lists the files in
`DiskCryptorLib.inf`. DcsPkg used to vendor its own copy of `crypto_fast` instead,
with the CRT includes commented out and a few `_UEFI` branches added by hand; that
copy is gone.

What the EFI target needs from the library:

| | |
|---|---|
| `CL_SEH` | EDK2 has no structured exception handling. The only thing that wanted it is the Hyper-V AES probe in `xts_fast.c`, which cannot be made safe without it, so firmware falls back to trusting CPUID. |
| CRT headers | `cl_platform.h` decides once. Under `_UEFI` it includes `<intrin.h>`, which resolves to the host's shim - UEFI types, rotate intrinsics, and `memcpy`/`memset` mapped onto `CopyMem`/`SetMem`. Sources include `cl_platform.h` rather than `<string.h>`/`<memory.h>`/`<stddef.h>`. |
| `<wmmintrin.h>` | Does not exist under EDK2. The AES-NI intrinsics are declared by the host's `emmintrin.h` shim instead. |
| `secure_alloc` | Argon2 allocates through `cl_alloc.h`; the host defines the pair. DcsPkg's is in `dc_host_efi.c`. |
| Argon2 threading | `argon2_mt.c` grew an `EFI_MP_SERVICES_PROTOCOL` branch, brought over from DcsPkg's copy - it is the only way Argon2 gets more than one core in firmware. |

Left out of the EFI source list on purpose: `cl_cpu.c`, `xts_serpent_avx.c` and
`xts_serpent_avx2.c`. Those tiers need per-file `/arch:` switches EDK2 gives no way
to set, so `CL_ENABLE_AVX`/`CL_ENABLE_AVX2` stay undefined and Serpent runs on the
SSE2 core - the same choice the kernel project makes, for a different reason.
`cl_cpu.c` exists only to probe for them.

`DcsPkg/Library/DiskCryptorLib/tests/build_efi_check.bat` compiles the module's
sources with the INF's flags, `/WX` included, without running the EDK2 build.
## Core selection and fallback

`xts_init(hw_crypt)` probes the CPU once and latches a function pointer per cipher;
`xts_set_key()` then captures those pointers into the key. Nothing is probed per call, so
the dispatch costs one indirect call per request.

**amd64**

| Cipher | preferred | falls back to | condition |
|---|---|---|---|
| AES | AES-NI, 8 blocks | Gladman scalar asm | `CPUID.01H:ECX.AES`, or `hw_crypt = 0` |
| Serpent | AVX2, 8 blocks | AVX 4 blocks (VEX-128), then SSE2, 4 blocks | each tier needs its feature bit + OSXSAVE + OS-enabled YMM state |
| Twofish | 4 blocks interleaved | — | no tiers; same core everywhere |

SSE2 is the Serpent floor rather than a tier that can fail: it is architectural on x86-64,
so no CPU that runs this code can lack it.

The AVX2 test is a real three-part check, not just a feature bit — `CPUID.07H:EBX.AVX2`,
plus `OSXSAVE`, plus confirmation that the OS has actually enabled XMM and YMM state
(`XGETBV` in user mode, `RtlGetEnabledExtendedFeatures` in the driver). Without that last
part a VEX-256 instruction faults on an OS that never enabled the state.

There is a **second, per-call** fallback in kernel builds: if
`KeSaveExtendedProcessorState` fails, that individual call runs on SSE2 instead of
corrupting another thread's YMM state.

AES-NI detection also carries the old Hyper-V workaround — some hypervisor configurations
mask the AES bit while the instructions still work, so on a confirmed Microsoft hypervisor
the code probes `AESENC` against a known answer under structured exception handling.

**ARM64**

NEON and the crypto extensions are both effectively architectural on Windows ARM64. Serpent
always uses NEON; AES uses the crypto extensions, with `IsProcessorFeaturePresent` checked in
user mode.

**The AVX tier is the same code as SSE2.** `xts_serpent_x86.inl` is compiled twice, once
plainly and once with `/arch:AVX`, which makes MSVC emit the identical intrinsics
VEX-encoded. It is still 128-bit and still four blocks wide; the gain is only that VEX's
three-operand form removes the register-copy moves two-operand SSE needs, and Serpent's
bitsliced S-boxes generate a lot of those. Worth ~2% here, which matters only on parts with
AVX but not AVX2 (Sandy/Ivy Bridge, Bulldozer/Piledriver, roughly 2011-2013) — anything newer
takes the AVX2 core. It exists so that hardware is no worse off than it was under the old
tree, whose "AVX" path was VEX-128 for the same reason.

### Argon2 and AVX2

Argon2 has an AVX2 `fill_segment`, and three separate things had to be true before any of it
ran. `opt_avx2.c` needed `/arch:AVX2`, or its whole body sits behind an inert
`#if defined(__AVX2__)`. `ref.c`'s dispatch was behind that *same* guard — and `ref.c` must
**not** get `/arch:AVX2`, since it has to keep running on CPUs without it, so the guard could
never be true there and the dispatch was compiled out regardless. And the runtime probe it
called, `HasSAVX2()`, was declared nowhere in the tree, so enabling the path did not even
link.

So `ref.c` now guards on `CL_ENABLE_AVX2` — the same project-level define the Serpent AVX2
tier uses — and calls `cl_cpu_has_avx2()`. Only `opt_avx2.c` is compiled with `/arch:AVX2`.
Measured at roughly **1.5×** against the SSE2 path it was stuck on (256 MiB, t=3: 464 ms →
318 ms), with identical digests.

The kernel project does not define `CL_ENABLE_AVX2`, so the driver stays on SSE2. That is
also the right answer independently: AVX2 in kernel mode would need a
`KeSaveExtendedProcessorState` bracket around every call, as `xts_fast.c` does for Serpent.

The probes themselves live in `cl_cpu.c`, which is compiled with no `/arch` switch at all. A
probe has to be callable on a CPU that lacks the feature it reports on, so it must never be
built with the switch that enables that feature — otherwise the compiler is free to emit the
very instructions the caller is asking about. `xts_serpent_avx_available()` and
`xts_serpent_avx2_available()` delegate there rather than each carrying their own copy inside
an `/arch`-compiled translation unit.

### AVX2 and the kernel build

The user-mode project defines `CL_ENABLE_AVX2` and compiles `xts_serpent_avx2.c` with
`/arch:AVX2`. The kernel project cannot: MSVC rejects `/arch:AVX2` together with `/kernel`.
`/arch:AVX` is rejected the same way, so the VEX-128 tier is user-mode only too. The driver
therefore runs the plain SSE2 Serpent core — 4 blocks wide, the same width the old driver's
"AVX" path actually used, so nothing is lost relative to before. Getting 8-wide Serpent into
the driver would mean hand-writing that core in assembler; that is the one optimisation left
on the table. AES and Twofish are unaffected: neither needs `/arch`.

Where AVX2 *is* used, `xts_fast.c` brackets it with `KeSaveExtendedProcessorState` in kernel
builds and falls back to SSE2 if the request fails. The old code used VEX-encoded
instructions in the driver with no such guard.

## Verification

Every primitive was checked against published known-answer vectors, using the existing
corpus in `crypto/tests`:

| | Vectors |
|---|---|
| Serpent-256 | Biham's full verified test-vector set |
| Twofish-256 | Schneier's published vectors |
| AES-256 | FIPS-197 |
| SHA-512, HMAC, PBKDF2 | published vectors |
| HMAC_DRBG | NIST CAVP `drbgvectors_pr_false` |
| XTS | IEEE 1619, all seven cipher combinations, hardware paths on and off |

Beyond that, all four Serpent cores (scalar, SSE2, VEX-128, AVX2) were cross-checked against
each other, and AES-NI against the Gladman core, on multi-sector buffers at a non-zero sector
offset.

`tests/` automates all of this; see `tests/README.md`.

```bat
tests\build.bat        :: builds each harness against both libraries
tests\run_tests.bat    :: crypto_lib vs crypto_fast, differential
tests\build_kat.bat    :: crypto_lib vs published vectors
```

`build_kat.bat` builds the `crypto/tests` corpus against this directory, applying the three
edits that needs to copies rather than to the originals.

`run_tests.bat` is the separate question of whether this library still produces exactly what
`crypto_fast` did — which is what decides whether existing volumes still decrypt. It compares
every ciphertext and every decryption byte for byte across 7 cipher combinations, both
`hw_crypt` modes, 24 keys, 9 lengths, 5 sector offsets and 4 buffer misalignments: 60,480
cases. Vectors alone would not catch a divergence that both libraries got wrong in the same
place, and equivalence alone would inherit any defect the old code has, so both halves are
needed.

## Performance

Measured on one machine, XTS, single-threaded, MB/s — old `crypto_fast` versus this
library. Treat as indicative, not a spec. Reproduce with `testsb_compare.py`, which
interleaves the two binaries sample by sample; measuring one library and then the other
lets thermal and load drift land entirely on one of them.

| Cipher | 512 B | 4 KiB | 64 KiB | 4 MiB |
|---|---:|---:|---:|---:|
| AES-256 | −8% | +5% | +6% | **+6%** |
| Twofish-256 | +55% | +55% | +55% | **+56%** |
| Serpent-256 | +79% | +76% | +79% | **+78%** |
| AES+Twofish | +49% | +51% | +52% | **+52%** |
| Twofish+Serpent | +58% | +65% | +66% | **+65%** |
| Serpent+AES | +67% | +72% | +72% | **+72%** |
| AES+Twofish+Serpent | +56% | +62% | +63% | **+63%** |

At 4 MiB that is 5325 → 5629 for AES, 352 → 627 for Serpent, 287 → 447 for Twofish and
154 → 250 for the triple cascade.

The size axis matters because real disk I/O is not 4 MiB at a time. Every core is flat
across it except AES, which is **8% slower than the old assembler on a single 512-byte
sector**. What is left there is not the tweak lookahead — that is guarded — but the
prologue: the first sector's tweak is a strictly serial fourteen-round AES that must finish
before the first block can be XORed, and on an 8-wide core given only 32 blocks there is
nothing to hide it behind. The old core is 4 blocks wide and has eight spare XMM registers
for the scheduler to interleave that derivation into the data rounds; this one does not.
Closing it would mean a separate narrow path for single-sector requests.

Serpent nearly doubles because the AVX2 core is genuinely 8 blocks wide, where the old
"AVX" assembler was VEX-encoded but still 128-bit and 4 wide.

Twofish beats the GPL assembler it had to replace, in portable C. Its round function is
four table lookups feeding an xor tree, two dependent adds and a rotate, with every round
depending on the one before, so run one block at a time the core mostly waits on that chain -
the loads have latency but plenty of spare throughput. Interleaving four independent blocks,
which XTS supplies for free, fills each other's stalls: 242 MB/s one-at-a-time, 383 at two
blocks, 450 at four. It needs more live registers than x86-64 has, but the spills land in L1
and cost far less than the stalls they replace.

AES beats the hand-tuned assembler, which took two rounds of work to reach. A
straightforward 8-wide intrinsics version lands around 4200 MB/s. Two things close the gap:

- **Deriving each sector's tweak one sector ahead.** That derivation is a full 14-round AES
  with a strictly serial dependency chain and nothing inside its own sector to overlap with.
  Issuing it early makes it independent of the surrounding work and it disappears into the
  AES pipeline. Worth ~12% at a 512-byte sector size. The Serpent and Twofish cores do the
  same. Every one of them guards it on whether a next sector actually exists — on the last
  sector the result is discarded, and unguarded that cost 12% of a single-sector AES request
  and 13% of a Serpent one. A wrong guard is invisible in bulk throughput and obvious at
  512 B, which is why `testsb_compare.py` sweeps request size.
- **Not holding the eight tweaks across the AES rounds.** Eight blocks plus eight tweaks plus
  a round key is seventeen XMM registers against sixteen available, so every group paid a
  spill and reload. Keeping only the group's first tweak and regenerating the other seven
  afterwards trades sixteen cheap integer-domain ops for those spills. Worth a further ~17%.

One MSVC detail matters for anyone touching this file: the round key must be a single
variable declared once in the function body, not a fresh `__m128i` inside each round's
block. Declared per-block, MSVC treats the fifteen declarations as fifteen separate values
and allocates them badly enough to undo the register-pressure fix entirely - measured at
4990 MB/s versus 5760 for otherwise identical code.

## Migrating a consumer

1. Change the include path from `crypto\crypto_fast` to `crypto_lib`.
2. Link `crypto_lib.lib` (user mode) or `crypto_lib_km.lib` (kernel).
3. Drop any direct `ClCompile` entries for `crypto\Argon2\*.c` — `crypto_lib` builds Argon2
   itself now, and keeping both would give duplicate symbols. `dcapi.vcxproj` and
   `driver.vcxproj` both have such entries today.
4. Drop references to `aes_padlock.h` / `aes256_padlock_*`.

`xts_init()` must be called once before `xts_set_key()`, as before — it is what picks the
cores, and `xts_set_key` captures the selection into the key.

## Two things fixed while wiring dcapi up

Both were latent: real, but only visible in a configuration nobody had built.

- **`cl_alloc.h` included `../dcapi/include/misc.h`** to get `secure_alloc`. That pointed the
  dependency the wrong way — an MIT library that is meant to stand alone reaching into a GPL
  consumer — and the declaration it picked up is decorated with `dc_api`, which is
  `__declspec(dllimport)` unless `DCAPI_DLL` is defined, which it is not when building here.
  So Argon2 compiled an import reference to a symbol dcapi defines locally, and linking
  `dcapi.dll` produced `LNK4217`/`LNK4286`. It linked, because the linker patches the
  indirection out, but every call went through an import thunk that should not have existed —
  and dcapi's definitions carry no `dc_api` either, so they were never exported in the first
  place. The two functions are now declared in `cl_alloc.h` directly, and the kernel branch
  no longer includes `../driver/include/misc_mem.h` either. **crypto_lib now includes nothing
  from outside itself.**
- **`<intrin.h>` was excluded on ARM64** in `serpent.c`, `twofish.c`, `sha512.c` and
  `cl_platform.h`, while those files use `_rotl`, `_rotr`, `_rotr64` and `_byteswap_uint64`.
  MSVC declares all of them for every target; excluding the header left them implicitly
  declared. That is a warning in Release and an error in the kernel Debug configuration, so
  `Debug|ARM64` simply did not build. `sha512.c`'s ARM64 `__stosd` shim is still needed and
  stays.

Neither changed behaviour: the differential suite's byte-exact stream hashes the same before
and after.

## Known gaps

- No 8-wide Serpent in the driver, and no VEX-128 tier there either; see above. Both need an
  `/arch` switch the kernel toolset refuses.
- No portable scalar AES. amd64 uses the Gladman core, ARM64 uses the crypto extensions, and
  Windows on ARM64 requires those. A target with neither would not link.
- Argon2's provenance via VeraCrypt should be diffed against upstream `phc-winner-argon2`
  before a formal release; see THIRD-PARTY-NOTICES.md.

