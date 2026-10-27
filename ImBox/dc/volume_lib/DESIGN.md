# DCrypt `volume_lib` — design

The driver's volume code, extracted into a library that also builds for user mode and EFI.
Depends on `crypto_lib` and nothing else.

**This is an extraction, not a redesign.** The driver is authoritative; dcapi is second; DCS
and ImBox are consumers. So the library gets the driver's functions, under the driver's
names, with the driver's semantics — and the consumers move onto them. The clean-sheet
alternative is in [DESIGN_old.md](DESIGN_old.md) and was rejected: it put the largest change
on the tree where a mistake is least recoverable.

## The rule that decides everything else

Minimise change in the driver. Concretely, three things follow:

- **Names do not change.** `cp_decrypt_header`, `io_write_header`, `dc_update_backup` keep
  their names in the library. The `_um` twins in dcapi and ImBox are the ones that get
  deleted.
- **Signatures change only where a type cannot cross the boundary.** In practice that is
  exactly one type: `dev_hook *`.
- **`ULONG *interrupt_cmd` stays as-is.** dcapi already passes its `volatile LONG *abort`
  through as `(ULONG*)abort`, so the two are compatible today. Unifying them is churn for
  nothing.

## What the numbers say

The one real design question is how the library refers to a device, since `dev_hook` is full
of `PDEVICE_OBJECT`, `KEVENT` and `LIST_ENTRY` and cannot move. Two ways to answer it, and
the cost is not close:

| Approach | Driver edits |
|---|---|
| Move the needed fields into a nested struct the library owns | **~400** — `hook->head_len` alone appears 109 times, `dsk_size` 71, `bps` 74, `tail_len` 47, `stor_len` 44, `stor_off` 32, `hdr_key` 22 |
| Leave every field where it is; change only the device parameter of the functions that move | **~40** — `io_read_header` 15, `io_write_header` 12, `dc_update_backup` 10, `io_read_header_full` 3 |

So: the fields stay in `dev_hook`, untouched. Only the parameter changes.

`io_hook_rw` (34 call sites) does not change at all — it stays in the driver as the driver's
own I/O primitive, and the library reaches it through a function pointer.

## The one prototype change: `dev_hook *` → `dc_dev *` + `dc_rw_fn`

```c
typedef struct _dc_dev {
    void     *ctx;          /* dev_hook* in the driver; whatever the consumer wants */
    u32       bps;
    u64       dsk_size;
    u32       head_len;
    u32       tail_len;
    u32       stor_len;
    u64       stor_off;
    u32       opt;          /* DC_DEV_BACKUP_HEADER | DC_DEV_STORAGE_ON_END */
    xts_key  *hdr_key;
} dc_dev;

typedef int (*dc_rw_fn)(void *ctx, void *buff, u32 length, u64 offset, int is_read);
```

**`dc_dev` holds no function pointers.** It says *what the device is*, never *how to reach
it*: the I/O callback is a second argument, and the wipe callback is a host hook.

That is not about the fifty bytes `dc_dev_of()` copies onto the caller's stack — against a
header read that runs a
KDF and hits a disk, the copy is free. It is about keeping one door open. The driver projects
`dev_hook` into a `dc_dev` per call today; if it later nests a `dc_dev` inside `dev_hook` and
hands out `&hook->dev`, the copy disappears and **the library does not change** — no per-hook
initialisation, no callback to keep valid, no redundant pointer per instance. A struct
carrying `rw` could still be nested, but not cleanly, and the option is free to preserve.

Being honest about what that option costs to exercise: nesting the geometry fields is still
about **445** edits (`head_len` 109 reads + 8 writes, `bps` 75, `dsk_size` 74, `tail_len` 52,
`stor_len` 53, `stor_off` 43, `hdr_key` 31). The hybrid buys the *option*, not a cheap future.

### `opt`, not `flags`

An earlier draft put `hook->flags` in `dc_dev`. That was wrong twice over. It is the worst
field in the driver to move — **224 reads and 57 writes** — and it is mount state, not
geometry, so it has no business in an interface ImBox and DCS must fill in.

The moved code only ever asks `flags` two questions, so `dc_dev` carries the two answers:

| `opt` bit | Driver derives it from |
|---|---|
| `DC_DEV_BACKUP_HEADER` | `hook->flags & F_HEAD_BACKUP` |
| `DC_DEV_STORAGE_ON_END` | `IS_STORAGE_ON_END(hook->flags)` — itself `F_ENABLED && !F_CDROM && !F_PROTECT_DCSYS && !F_NO_REDIRECT` |

`hook->flags` therefore never moves, and `F_*` never appears in the library. This also settles
what was open question 1.

### Call sites

One helper projects the live fields into a `dc_dev` the caller owns:

```c
void dc_dev_of(dev_hook *hook, dc_dev *dev);   /* fills the caller's dc_dev */
```

The `dc_dev` is a local, not a member of `dev_hook`. A member would be a second copy of state
the hook already holds, shared by every caller and outliving each of them; since the
projection is refreshed rather than maintained, whether it was current would depend on who
called last. A local is private, obviously short-lived, and costs one struct of stack.

```c
  dc_dev dev;
  ...
  dc_dev_of(hook, &dev);
- io_write_header(hook, backup_pos, hcopy, bak_key, NULL, flags, NULL);
+ io_write_header(&dev, dc_hook_rw, backup_pos, hcopy, bak_key, NULL, flags, NULL);
```

Fill it once per operation and pass `&dev` from there on. `dc_apply_layout()` is the
exception: it is what moves the layout, so `head_len`, `stor_off`, `stor_len`, `tail_off` and
`F_HEAD_BACKUP` all change between its writes, and it refills before each one.

~40 sites. Not one existing `hook->field` access anywhere in the driver changes.
`dc_hook_rw` is a one-line thunk over `io_hook_rw`, which itself is untouched — all 34 of
**its** call sites stay exactly as they are.

**This works only because the moved functions never write back to the hook.** Checked:
`io_read_header` and `io_read_header_full` allocate and return through out-parameters;
`io_write_header` writes nothing to the hook; `dc_update_backup` reads `hook->hdr_key` as a
fallback and does not assign it. The projection is read-only, and that is a property to keep —
anything moved later that needs to mutate device state must return the value instead.

Why `dc_dev_of()` refreshes rather than being filled once at mount: `head_len`, `tail_len`
and `hdr_key` genuinely change during a volume's life — `dc_update_volume` resizes,
transcryption swaps keys. A snapshot that outlived the change would be a silent correctness
bug, which is also why the projection is scoped to one operation rather than to the hook.

## Host hooks

Only four driver facilities are used by the code that moves, and one is optional:

| Driver | Library hook | Used by |
|---|---|---|
| `mm_secure_alloc` / `mm_secure_free` | `host->secure_alloc` / `secure_free` | all three files |
| `cp_rand_bytes` | `host->rand_bytes` | `header_io.c`, `misc_volume.c` |
| `cp_rand_add_seed` | `host->rand_add_seed` | `header_io.c` (one call) |
| `DbgMsg` | `host->log` (may be NULL) | all three |
| `dc_wipe_process` | `host->wipe` (may be NULL) | `dc_change_pass_bak` only; its `wipe_ctx*` stays an opaque parameter, as it already is in the driver |

See [dc_dev.h](dc_dev.h) for the declarations. `dc_lib_init()` installs the host and calls
`xts_init()`.

---

# Migration

Eight steps. The driver builds and runs after every one of them.

## Step 1 — move functions around inside the driver  ✅ done

No behaviour change, no signature change, nothing leaves the driver yet. Only: separate the
code that will move from the code that will stay, so that step 3 is a file move rather than a
merge.

`crypto_head.c` needs nothing — **it has zero `dev_hook` references** and is already
library-shaped. `header_io.c` is nearly so. `misc_volume.c` is the one that has to be split,
because it mixes device-name-level entry points with header-level helpers:

| `misc_volume.c` | Disposition | Why |
|---|---|---|
| `dc_backup_header` | **stays** | `dc_find_hook`, `busy_lock`, `F_*` checks, `dc_fill_device_info` |
| `dc_restore_header` | **stays** | same |
| `dc_update_header` | **stays** | same |
| `dc_change_pass` | **stays** | same |
| `dc_format_start` / `_step` / `_done` | **stays** | device lifecycle |
| `dc_restore_end_sectors`, `dc_save_end_sectors` | **stays** | `NTSTATUS`, hook internals |
| `dc_update_volume` | **stays** | `NTSTATUS`, PnP-adjacent |
| `dc_update_key_slots` | **moves** | already hook-free (`static`; becomes exported) |
| `dc_change_slot_pass` | **moves** | takes `dev_hook*` and **never uses it** — the parameter is dropped, not translated |
| `dc_change_pass_bak` | **moves** | needs `dc_dev` + the optional wipe callback |
| `dc_update_backup` | **moves** | needs `dc_dev` |

The movers go into a new `driver/volume_head.c` in this step, still compiled into the driver,
still called the same way. At the end of step 1 the driver is byte-identical in behaviour and
the future library is three files: `crypto_head.c`, `header_io.c`, `volume_head.c`.

### Outcome

Done. `driver/volume_head.c` (384 lines) and `driver/include/volume_head.h` exist and are in
`driver.vcxproj`; `misc_volume.c` went from 1616 to 1267 lines.

Verified rather than asserted — `tests/step1_verify.py` extracts each function from the
pre-move `misc_volume.c` and from `volume_head.c` and compares line for line:

```
  dc_update_key_slots     57 lines, identical
  dc_change_slot_pass     90 lines, identical
  dc_change_pass_bak      88 lines, identical
  dc_update_backup       110 lines, identical
  misc_volume.c           = original minus the four bodies
```

The single permitted edit is `dc_update_key_slots` losing `static`, because `dc_change_pass`
stayed behind and still calls it. Wiring: `misc_volume.c` and `enc_dec.c` include
`volume_head.h`; `dc_update_backup`'s declaration moved out of `misc_volume.h` (it was the
only one of the four declared in a header, the other three were file-local).

`tests/build_driver_check.bat` compiles the driver's whole `Release|x64` set — **39 of 39,
zero failures**, including the eight vendored Argon2 sources the project pulls in from
`crypto\Argon2`.

It takes the file list from `driver.vcxproj` rather than globbing `*.c`, so that
`ExcludedFromBuild` is honoured. That distinction is load-bearing: `base64.c` is excluded
from every Release configuration and is pulled into `verify.c` by `#include "base64.c"`. It
has no includes of its own and relies on `verify.c`'s, so compiling it standalone fails for
reasons that have nothing to do with anything.

## Step 2 — expand prototypes  ✅ done

Now the signatures, still inside the driver, still compiling and testable there.

| Function | From | To |
|---|---|---|
| `io_read_header` | `(dev_hook *hook, …)` | `(dc_dev *dev, dc_rw_fn rw, …)` |
| `io_read_header_full` | `(dev_hook *hook, …)` | `(dc_dev *dev, dc_rw_fn rw, …)` |
| `io_write_header` | `(dev_hook *hook, …)` | `(dc_dev *dev, dc_rw_fn rw, …)` |
| `dc_update_backup` | `(dev_hook *hook, …)` | `(dc_dev *dev, dc_rw_fn rw, …)` |
| `dc_change_pass_bak` | `(dev_hook *hook, …, wipe_ctx *wipe, …)` | `(dc_dev *dev, dc_rw_fn rw, …, void *wipe, …)` — the parameter keeps its name; calling it `wipe_ctx` shadows the *type* `wipe_ctx` and the cast stops parsing. Wiping goes through `host->wipe` in step 3. |
| `dc_change_slot_pass` | `(dev_hook *hook, dc_header *header, …)` | `(dc_header *header, …)` — unused parameter removed |
| `dc_update_key_slots` | `static int` | exported |

Everything in `crypto_head.c` — all thirteen functions — changes **not at all**.

### Outcome

Done, and the driver still compiles clean — **39 of 39**.

| | |
|---|---|
| `dc_dev`, `dc_rw_fn`, `DC_DEV_*` | in `volume_lib/dc_dev.h`; `$(SolutionDir)volume_lib` added to the driver's include path in all four configurations, so step 3 needs no further include change |
| `dev_hook` | gained one member, `dc_dev lib_dev` |
| `devhook.c` | `dc_dev_of()` — projects six fields, sets `ctx`, derives `opt` |
| `device_io.c` | `dc_hook_rw()` — one-line thunk; `io_hook_rw` and all 34 of its call sites untouched |
| call sites | **34** rewritten: `enc_dec.c` 16, `misc_volume.c` 16, `mount.c` 2 |

`tests/step2_verify.py` checks what compiling cannot — that the bodies still mean the same
thing:

```
  ok   dc_dev_of projects 6 fields, each from its own name
  ok   header_io.c    reads only projected fields (bps, ctx, head_len)
  ok   volume_head.c  reads only projected fields (ctx, dsk_size, head_len, opt, stor_len, tail_len)
  ok   both hook->flags tests became dc_dev.opt tests
  ok   dc_dev_of derives opt from F_HEAD_BACKUP and IS_STORAGE_ON_END
  ok   every call site gained exactly one argument (dc_change_slot_pass lost one)
```

The argument-count check is the useful one: it parses each call with depth-aware comma
counting and insists every site gained *exactly* one argument — a dropped or duplicated
parameter would compile fine if the types happened to line up, and this catches it.

Two things surfaced while doing it, both now settled:

- **`hook->hdr_key` was already dead.** `dc_update_backup` carries a comment saying it falls
  back to it, but the code validates the caller's `bak_key` and fails if it is missing.
  `dc_dev` therefore has no `hdr_key` field, and the stale comment is gone — which also keeps
  31 accesses out of the way of ever nesting the struct.
- **`stor_off` is not read by any moved function.** Kept in `dc_dev` because `dc_mount_io`
  needs it in step 4, and marked as such.

Add `dc_dev_of()`, give each operation a `dc_dev` local, and rewrite the ~40 call sites. This
is the step to run the full driver test pass on, because it is the last one where a mistake
is still inside familiar code.

## Step 3 — move them to the library  ✅ done

A file move plus the host-hook substitution:

| Driver file | Becomes | Host substitutions |
|---|---|---|
| `crypto_head.c` / `.h` | `volume_lib/crypto_head.c` | `mm_secure_alloc/free`, `DbgMsg` |
| `header_io.c` / `.h` | `volume_lib/header_io.c` | + `cp_rand_bytes`, `cp_rand_add_seed`, `io_hook_rw` → `dev->rw` |
| `volume_head.c` (from step 1) | `volume_lib/volume_head.c` | + `dc_wipe_process` → `dev->wipe` |

`volume_header.h` moves from `_include_/` into `volume_lib/` at the same time, and the copies
in `DcsPkg/…/include/` and `ImBox/dc/include/` are deleted in steps 7 and 8.

The driver still compiles these files at this point — same sources, new location — so the move
is verifiable on its own before anything is excluded.

### Outcome

Done. The driver compiles the three files from `volume_lib\` plus `dc_host.c` — **40 of 40**.

Host substitution, a rename of the same arity at each site so the code keeps its shape:

| Driver | Library | Sites |
|---|---|---|
| `mm_secure_alloc` / `mm_secure_free` | `dc_alloc` / `dc_free` | 29 |
| `cp_rand_bytes` | `dc_rand` | 7 |
| `cp_rand_add_seed` | `dc_rand_seed` | 1 |
| `dc_wipe_process` | `dc_wipe` | 1 |
| `DbgMsg` | *kept* — now a macro onto `dc_log` | 35 untouched |

`tests/step3_verify.py` applies those renames to the pre-move text and requires the result to
equal the moved file exactly:

```
  ok   crypto_head.c    identical after 4 substitutions
  ok   header_io.c      identical after 22 substitutions
  ok   volume_head.c    identical after 11 substitutions
  ok   no driver headers, no driver facilities referenced
```

Three decisions the move forced:

- **`init_header_v2` went back to the driver.** It takes a `crypt_info`, a driver type, and
  composing a header is precisely what kernel mode is not supposed to do in this split. It now
  lives in `misc_volume.c` next to the other driver-side volume work, which is where the tier
  table always said it belonged — the move just made it concrete.
- **`dc_lib_init()` no longer calls `xts_init()`.** The driver already calls it, with an
  `hw_crypt` flag taken from configuration that is not loaded at the point the host must be
  installed. Folding it in would have invented an ordering constraint that does not exist, so
  installing the host and initialising crypto_lib stay separate.
- **`dc_host.log` takes a `va_list`** rather than being variadic. The kernel has no usable
  `vsnprintf` at arbitrary IRQL, and `vDbgPrintEx` consumes a `va_list` directly, so
  forwarding one costs the driver nothing.

### Two bugs the compile-only harness missed

Both surfaced in Visual Studio, not here, and both were mine:

- **`init_header_v2` came back carrying a library name.** It was substituted while still in
  `header_io.c`, then moved back to `misc_volume.c` — with `dc_rand` in it instead of
  `cp_rand_bytes`. Order matters: move first, substitute second.
- **`cp_rand_bytes` is `int(u8*, int)` and `cp_rand_add_seed` is `void(void*, int)`**, neither
  matching the `dc_host` fields. They were assigned to the struct directly; both need thunks,
  like `log` and `wipe` already had.

The harness missed them because it ran without the real build's elevated warnings. It now
elevates the mismatched-parameter family (`C4020/4024/4028/4029/4047/4090/4113/4133`), which
catches the second, and checks `C4013` separately, which catches the first — verified by
reintroducing the `dc_rand` bug and confirming the harness fails.

Blanket `/WX` is deliberately **not** used: it trips on pre-existing `C4101`s in files this
migration never touches, because the real build has `$(DDK)\inc\crt` on its include path and
the harness does not. One known consequence: `abs` in `crypto_head.c` is implicitly declared
and always has been — identical `C4013` before and after the move. Harmless in the driver,
but it is a CRT dependency that EFI will not satisfy, so it needs an answer in step 7.

### Still depending on the tree

The library is not standalone yet, and should not pretend to be. It includes
`include/defines.h`, `include/dcconst.h`, `_include_/volume_header.h`, `crypto/crc32.h` and
`crypto/crypto_fast/sha512_pkcs5_2.h` — all shared or crypto headers, none of them driver
headers.

`volume_header.h` was listed for this step but **has not moved**. It is included by ten files
in dcapi alone, so moving it means touching dcapi's include path — which is step 6. Doing it
here would have dragged a consumer into a driver-only step for no benefit.

### Licence

**`volume_lib` is MIT**, like `crypto_lib`. It did not start that way.

It was extracted from the driver, which is GPL-3 with ntldr's copyright on much of it, so the
library inherited that. Before the licence changed, every file was compared function by
function against **1.1.846.118**, the last release he authored, to find what was still his
expression rather than a description of the same job.

The answer was about 44 lines of roughly 1,570, nearly all of it in `header_io.c`:
`is_volume_header_correct` was his almost verbatim, the cores of `io_read_header` and
`io_write_header` were his inside much larger functions, and fifteen of his comments had
survived word for word. Those were rewritten. Everything else — all of `head_util.c`,
`dc_host.c`, `dc_buffer.c`, the key slots, the KDF machinery, partial header updates — had no
counterpart in his tree at all.

The rewrite changed expression, not behaviour: `tests/golden_test.c` folds every byte the
header paths write and every code they return into one SHA-512, and the digest taken before
the rewrite is unchanged after it. A header that differs by one byte is a volume that no
longer opens, so that is checked rather than asserted.

`volume_lib/PROVENANCE.md` has the per-function evidence. Note that the library still uses
DiskCryptor's headers — `volume_header.h`, `dcconst.h`, `defines.h` — which stay under the
main tree's licence: MIT describes this body of code, not a self-contained distribution.

For ImBox this was never a blocker in the first place: `ImBox.cpp` carries a GPL-3.0-or-later
header of its own, and `dc/` was already DiskCryptor code, so GPL-3.0-only would have linked
fine. MIT is about future reuse, not about unblocking a consumer.

## Step 4 — add what kernel mode does not have  ✅ mostly done

Only now, and only what a consumer actually needs. Each of these exists in dcapi, ImBox or
DCS today and has no kernel counterpart:

| New in the library | Needed by | Where the logic comes from |
|---|---|---|
| `cp_encrypt_header(header, len, hdr_key, out)` | dcapi, ImBox | the sealing half of `io_write_header`, factored out so it can target a buffer instead of a device |
| `cp_decrypt_header_with_key(blob, len, hdr_key, out)` | ImBox, DCS | buffer-only form of `io_read_header_full` |
| `cp_get_slot_info` / `cp_get_slot_payload` / `cp_set_slot` | dcapi, ImBox | dcapi's `dc_get_slot_info` / `_payload` / `dc_set_slot`, renamed to the `cp_` family |
| `cp_has_key_slots` | dcapi, ImBox | dcapi's `dc_has_key_slots` |
| `cp_read_ext_header` / `cp_write_ext_header` / `cp_ext_header_crc` | dcapi | dcapi's `read_ext_header` / `write_ext_header` / `calculate_ext_header_crc_um` |
| `cp_kdf_list(selector)` | dcapi | dcapi's `dc_default_kdfs` / `dc_all_kdfs` tables, which `tpm_sup.c` currently reaches with a bare `extern` |
| `dc_mount_io` / `dc_crypt_io` | DCS, ImBox | **from DCS**, not the driver — see below |
| `dc_lib_init(host, hw_crypt)` | all | new; replaces `dc_init_crypto` in dcapi and ImBox |

`dc_mount_io` is the one place the driver is not the source. Its equivalent is spread through
`readwrite.c` as async, chunked, IRP-bound code that cannot move; DCS's `dc_mount_io` is the
same geometry expressed synchronously, and is the best-documented version of that logic in any
of the four trees. So it is lifted from DCS, and `readwrite.c` is left alone — see
[Deferred](#deferred).

### Outcome

Added, in `head_util.c` and `dc_buffer.c`:

| | |
|---|---|
| `cp_encrypt_header` | seals a header into a buffer |
| `cp_decrypt_header_with_key` | unseals one with a key already known |
| `cp_has_key_slots`, `cp_get_slot_info`, `cp_get_slot_payload`, `cp_set_slot` | ported from dcapi, renamed into the `cp_` family |
| `cp_ext_header_crc` | |
| `cp_kdf_list` | replaces dcapi's two tables and `tpm_sup.c`'s bare `extern` |
| `dc_dev_from_buffer`, `dc_buffer_rw` | a `dc_dev` over memory, so a blob needs no second set of entry points |

Kernel configuration compiles — 42 of 42 — and `tests/run_step4_test.bat` exercises them for
real in user mode, 25 checks, all passing. The one that matters:

```
  ok   cp_encrypt_header == io_write_header, byte for byte
```

A header sealed by dcapi has to be one the driver would have written. The test seals with
`cp_encrypt_header`, then writes the same header through `io_write_header` over a
buffer-backed `dc_dev`, and compares. That is the compatibility property the whole exercise
rests on, and it is now checked rather than assumed.

**The test caught a real bug.** `cp_decrypt_header_with_key` decrypted the whole
`DC_AREA_SIZE`, salt included — but the salt is never encrypted, so it came back as rubbish.
The driver avoids this by decrypting into a scratch buffer and copying back only from `->sign`
onward (`cp_try_decrypt_header`); the new function decrypts in place, so it has to put the
salt back explicitly. The header still validated either way, because the v2 CRC excludes the
salt — which is exactly why this would have survived a weaker test and then failed on the
next unlock.

### Two things left out

- **`cp_read_ext_header` / `cp_write_ext_header`.** They are not header code, they are a
  serialisation format, and dcapi implements them on `misc/SVariant.c` — 536 lines pulling in
  `<stdlib.h>` and `<string.h>`, which would follow the library into kernel and EFI builds
  that cannot use them, for one consumer. They stay in dcapi. `cp_ext_header_crc` and
  `get_ext_header`, the parts that are about the header format, are in the library.
- **`dc_mount_io` / `dc_crypt_io` — deferred to step 7.** Porting them needs a volume-state
  struct (DCS's `mount_inf` minus its EFI members) designed with no consumer at hand to
  validate it, and it is precisely the code where DCS and the driver are known to disagree:
  DCS re-enters `dc_mount_io` for redirected ranges, mapping them twice, where the driver maps
  once, and DCS assumes `tail_len == head_len` where the driver does not. Writing it blind now
  means guessing which behaviour is correct. Step 7 converts DCS, so that is where both the
  struct and the discrepancy can be settled against something that runs.

## Step 5 — exclude the driver files, link the library  ✅ done

Remove `crypto_head.c`, `header_io.c` and `volume_head.c` from `driver.vcxproj`; add
`volume_lib_km.lib`. Add `dc_lib_init()` to driver startup with the driver's own
`mm_secure_alloc` / `cp_rand_bytes` / `DbgMsg`.

If steps 1–3 were done properly this is a project-file change and one init call. That is the
whole point of splitting it this way.

### Outcome

Done, and it was a project-file change — the driver now links what it used to compile. Also
done in the same step, at your request: the driver moved off `crypto_fast` onto `crypto_lib`,
and all four library projects are in `dcrypt.sln`.

```
crypto_lib_km.vcxproj -> Bin\Release_amd64\crypto_lib_km.lib
volume_lib_km.vcxproj -> Bin\Release_amd64olume_lib_km.lib
driver.vcxproj        -> Bin\Release_amd64\dcrypt.sys      (signed)
```

Both **x64 and ARM64** build clean end to end. `tests/build_sln.bat` drives MSBuild over the
real projects in dependency order and is now the authority;
`tests/build_driver_check.bat` remains the quick way to see a kernel compile error.

New: `volume_lib.vcxproj` (v143, for dcapi in step 6) and `volume_lib_km.vcxproj`
(WindowsKernelModeDriver10.0). Both carry a `DcCryptoDir` switch so Win32 would build against
`crypto_fast` — see below.

### Four things the switch to crypto_lib needed

None were surprises exactly, but none were free either:

- **PadLock.** `crypto_lib` dropped it. The driver only used it to set a UI capability flag,
  so the probe is now `#ifdef _M_IX86`; on x64 the flag is simply cleared.
- **`xts_serpent_avx_available()` does not exist in a kernel `crypto_lib` build** — MSVC will
  not take `/arch:AVX` with `/kernel`, so that tier is not compiled. The driver used it for
  another capability flag, and now calls `cl_cpu_has_avx()` instead. That is precisely the
  function added when `cl_cpu.c` was split out for the Argon2 work: a probe compiled without
  `/arch`, so it is safe to call on a CPU that lacks the feature. The flag now reports what
  the CPU can do rather than which Serpent tier got compiled, which is what the flags beside
  it (AES-NI, SSE2) have always meant.
- **`#include "../Argon2/argon2.h"`** worked only because `crypto_fast` keeps Argon2 as a
  sibling; `crypto_lib` keeps it inside. Now `"Argon2/argon2.h"`, which resolves under either
  when the parent is on the include path. Same for `"..\crc32.h"` -> `"crc32.h"` in four
  driver files.
- **Win32.** `crypto_lib` has no 32-bit x86 build, so the driver's `Release|Win32` was set up
  to stay on `crypto_fast_km` via a `DcCryptoDir` switch, and `volume_lib` gained Win32
  configurations to match.

  That accommodation turns out to protect nothing. **`Release|Win32` cannot build at all with
  the installed WDK**: `WindowsDriver.common.targets` rejects it with *"'Win32' is not a valid
  architecture for Kernel mode drivers"*, and the only architectures it lists are x64 and
  ARM64. `crypto_fast_km.vcxproj` is untouched by this migration, so that is pre-existing, not
  something the switch caused. The switch is left in place because it costs nothing and is
  correct under an older WDK — but if 32-bit drivers are not coming back, `Release|Win32`
  could be dropped from `driver`, `crypto_fast_km` and `volume_lib*` and the switch with it.

## Step 6 — dcapi  ✅ done

`dcapi/dc_header.c` is deleted. Its exported names stay, as one-line forwarders, so that
`gui/` and every external consumer of `dcapi.dll` keep building unchanged:

| dcapi export | Forwards to |
|---|---|
| `calculate_header_crc_um` | `calculate_header_crc` |
| `is_volume_header_correct_um` | `is_volume_header_correct` |
| `argon2_mk_params_um` | `argon2_mk_params` |
| `dc_derive_key_um` | `dc_derive_key` |
| `dc_try_decrypt_header_um` | `try_decrypt_header` |
| `dc_decrypt_header` | `cp_decrypt_header` over a buffer-backed `dc_dev` |
| `dc_encrypt_header` | `cp_encrypt_header` |
| `dc_load_header_file` / `dc_save_header_file` | `io_read_header` / `io_write_header` over a file-backed `dc_dev` |
| `dc_get_key_slot_size` | `cp_get_key_slot_size` |
| `dc_has_key_slots`, `dc_get_slot_info`, `dc_get_slot_payload`, `dc_set_slot` | the `cp_` forms from step 4 |
| `dc_wrap_header_key` | `cp_wrap_header_key` |
| `read_ext_header`, `write_ext_header`, `calculate_ext_header_crc_um` | the `cp_` forms from step 4 |

**The IOCTL wrappers in `drv_ioctl.c` do not change.** `dc_backup_header`, `dc_restore_header`,
`dc_update_header` and `dc_change_password` there are thin `DeviceIoControl` calls that share
names with the driver's real implementations. They keep their names and keep forwarding to the
driver. The library never exports those names — which is exactly why the split in step 1 put
the device-name-level entry points on the "stays" side.

dcapi needs a file-backed `dc_dev` (for header backup files) and a buffer-backed one (for
`dc_decrypt_header` on an in-memory blob). Both are about fifteen lines.

### Outcome

Done. `dcapi/dc_header.c` implements no header logic any more — it is the translation layer
between dcapi's exported names and `volume_lib`. All **19** functions declared in
`dc_header.h` are still defined and still have their old signatures, checked mechanically.

```
crypto_lib_km.lib -> volume_lib_km.lib -> dcrypt.sys
crypto_fast.lib   -> volume_lib.lib    -> dcapi.dll -> dcrypt.exe
```

x64 and ARM64 both build end to end, **and `gui` builds untouched** — which was the point of
keeping every name: nothing outside dcapi had to change.

In the end neither a file-backed nor a buffer-backed `dc_dev` was needed here.
`dc_load_header_file` already reads the file itself and hands over a buffer, and
`cp_decrypt_header` works on a buffer directly, so `dc_decrypt_header` is a `memcpy` plus one
library call. `dc_dev_from_buffer` stays for ImBox in step 8.

### The one library change this forced

`cp_decrypt_header` and `cp_decrypt_header_with_kdf` gained an optional **`out_dk`** — the key
material that actually opened the header, which is the password-derived key on the header path
and the key recovered from a slot on the slot path.

The GUI caches it (`cached_primary_hdk` in `prc_header.c`) so a second unlock of the same
volume does not pay for the KDF again. Without it dcapi had two options, both bad: keep its
own copy of the whole KDF/slot/cipher search, or re-derive afterwards and pay for Argon2
twice. An optional out-parameter costs the driver five `NULL`s.

### Notes

- **`WIN32_LEAN_AND_MEAN`** is now defined in `dc_header.c`. `misc/SVariant.h` declares its own
  `VARIANT`, and the full `windows.h` pulls `ole2.h` -> `oaidl.h`, which declares another;
  nothing in the file needs OLE. `read_ext_header`/`write_ext_header` also use the struct tag
  rather than the bare typedef.
- **`dc_decrypt_header` still does the tail decrypt and slot copy itself** — about six lines
  that `io_read_header_full` also has. Folding them together would mean giving `io_read_header`
  the same `out_dk` parameter and editing its 15 driver call sites, which is not worth six
  lines.
- **The IOCTL wrappers in `drv_ioctl.c` are untouched**, as planned. `dc_backup_header`,
  `dc_restore_header`, `dc_update_header` and `dc_change_password` there share names with the
  driver's implementations and keep forwarding to the driver.
- `tests/step3_verify.py` now also knows about the `out_dk` delta, so it stays a live
  regression check rather than a step-3 snapshot.

### Argon2, and dcapi onto crypto_lib

`crypto_fast` never built Argon2, so dcapi and the driver each compiled `crypto\Argon2\*.c`
themselves. `crypto_lib` does build it — which meant that from step 5 the **driver had been
linking `crypto_lib_km.lib` and compiling its own copy at the same time**. It linked only
because the object files win and the library members are never pulled; it is exactly the
duplicate `crypto_lib/README.md` warns about, and step 5 introduced it.

Both are now cleaned up, and dcapi moved onto `crypto_lib` to make it possible:

| | x64 / ARM64 | Win32 |
|---|---|---|
| driver | `crypto_lib_km.lib` | `crypto_fast_km.lib` |
| dcapi | `crypto_lib.lib` | `crypto_fast.lib` |
| volume_lib | built against `crypto_lib` | built against `crypto_fast` |
| `crypto\Argon2\*.c` in driver/dcapi | **excluded** | still compiled |

Win32 keeps its own Argon2 because `crypto_lib` has no 32-bit build. Verified by deleting the
intermediate directories and rebuilding: **zero** `argon2*.obj` under `obj\dcapi` or
`obj\driver`, and `crypto_lib.lib` does carry `argon2.obj` and `argon2_mt.obj`.

`"..\crc32.h"` became `"crc32.h"` in six more dcapi files, for the same reason it did in the
driver: the relative form only resolved because `crypto\crypto_fast` happened to sit next to
`crypto\crc32.h`, and `crypto_lib` keeps its own copy inside instead.

x64 and ARM64 build end to end, `gui` included.

### Fallout in crypto_lib

Linking dcapi against `crypto_lib` surfaced two latent problems there, both fixed and both
written up in `crypto_lib/README.md`: `cl_alloc.h` was including `../dcapi/include/misc.h`
and picking up a `dllimport` declaration (the `LNK4217`/`LNK4286` warnings), and `<intrin.h>`
was excluded on ARM64 in four files that use MSVC rotate intrinsics, which broke
`Debug|ARM64`. crypto_lib now includes nothing from outside itself, and its differential
suite hashes identically before and after.

`abs()` in `crypto_head.c` went the same way — it had never been declared, which was harmless
in the driver, an error once `C4013` is elevated, and unavailable in EFI. It is now a local
`cp_abs` macro, which also settles the portability item step 3 left open.

All four configurations build: **Release and Debug, x64 and ARM64**.

### dcapi Win32 does not build, and did not before

`crypto_fast` and `volume_lib` both build fine for Win32; `dcapi` does not, on three counts,
none of them touched by this migration and none related to Argon2 or crypto:

- `drvinst.c(456)` uses `#elifdef`, which needs `/std:clatest`
- `_include_/bootloader.h(225)` — `static_assert` that `bd_data` is 1657 bytes, which fails
  on a 32-bit layout
- warnings-as-errors in `misc/fat_sup.c` and `misc/SVariant.c`

Checked that the include directories added for `volume_lib` cannot shadow anything dcapi
already included, so this is pre-existing. Left alone: the `bd_data` assert in particular is
about MBR bootloader compatibility and is not something to "fix" in passing.

## Step 7 — DCS  ✅ done

Delete `DiskCryptorLib/dc_header.c` and the `include/` copies of `volume_header.h` and
`defines.h`. `dc_io.c` keeps its `BlockIo` shim, now filling a `dc_dev`; `dc_mount_io` and
`dc_crypt_io` move into the library in step 4 and DCS calls them there.

EFI-specific: no CRT, and the local `intrin.h` / `emmintrin.h` shims stay in the DCS project's
include path rather than moving into the library. Watch image size — if it matters, the
`DCV_ENABLE_ADMIN` equivalent (below) lets backup and restore be left out.

### Outcome — prepared, awaiting a build

DCS compiles sources directly rather than linking a `.lib`, so `volume_lib` is reached through
a **junction** at `DcsPkg/Library/DiskCryptorLib/volume_lib`. EDK2 keeps `[Sources]` inside the
module directory, and the alternative — a fourth copy of the code — is the thing this exercise
removes. `ci_build_dcs.cmd` already junctions DcsPkg itself; it now creates this one too, so a
fresh checkout works.

| | |
|---|---|
| deleted | `DiskCryptorLib/dc_header.c` |
| added to `[Sources.common]` | `volume_lib/dc_host.c`, `crypto_head.c`, `header_io.c`, and `dc_host_efi.c` |
| `DcsPkg.dec` | `Library/DiskCryptorLib/include` and `.../crypto_fast` added, so the library's bare `"defines.h"`, `"sha512_pkcs5_2.h"` and `"crc32.h"` resolve |
| `include/volume_header.h` | replaced by DCrypt's |

Only **three** functions from DCS's `dc_header.c` were used anywhere else, which is why the
conversion is small: `cp_get_min_header_len` and `dc_decrypt_header` in `DcsDiskCryptor.c`, and
`dc_derive_key` in `DcsTpmSupport.c`. Everything else in that file was internal.

`dc_host_efi.c` holds the EFI host — `AllocatePool` with the size stashed in front so the free
path can wipe it, and `GetRandomNumber64` from RngLib — plus one adapter. DCS wants the cipher
back, which `cp_decrypt_header` does not report; it does not need to, because a header is
sealed with the cipher named in its own `alg_1`, so the adapter reads it off the decrypted
header.

`cp_decrypt_header` gained an optional **`out_slot`**, the same shape as step 6's `out_dk`.
DCS shows which key slot opened the volume, and dropping that to save five `NULL`s in the
driver would be losing information the UI displays.

### volume_header.h had drifted, in one alarming way

DCS's copy and DCrypt's were **not** the same file. The struct layouts were identical — so
headers written by one have always been readable by the other — but:

- **`0x40` meant two different things.** `VF_NO_HIBER` in DCrypt, `VF_USE_SLACK` in DCS, with
  `VF_TRY_SHRINK` at `0x80` besides. DCS annotated its pair as "request-only, not stored in
  the header", so nothing was actively misread, but the same bit in the same `flags` word
  carrying two meanings is a trap set for whoever touches it next.
- `dc_slot_info.format` in DCS vs `.type` in DCrypt — same offset, same width, different name.
- `SECTOR_SIZE` and `CD_SECTOR_SIZE` commented out in DCS; `ALL_KEY_SLOTS`, `PF_*` and two
  `static_assert`s missing.

Checked before replacing: `VF_USE_SLACK`, `VF_TRY_SHRINK`, `VF_NO_HIBER` and `->format` are
referenced **nowhere** in DcsPkg, and nothing in edk2 or DcsPkg defines `SECTOR_SIZE`. So
adopting DCrypt's copy is safe, and this is exactly the drift the extraction exists to stop.

### IA32

Dropped. It was a declared target building against `crypto_small`, whose API `volume_lib` is
not written to — but it is deprecated and no longer built, and there is no
`ci_build_dcs_ia32.cmd`. The `SMALL` switch is gone from `dc_header.h` with it.

`DcsPkg.dsc` still lists `IA32` in `SUPPORTED_ARCHITECTURES` and the INF still has a
`[Sources.IA32]` block pointing at `crypto_small`; that block no longer has a header
implementation behind it, so the declaration is now untrue. Removing IA32 from both is a
tidy-up worth doing, but it is DcsPkg's call, not this migration's.

### Not verified

**Nothing here has been compiled.** EDK2 is present and the toolchain works, but the build was
left to you. What *was* checked statically: no references to the removed names survive, the
INF source list is consistent, the include paths cover every bare include `volume_lib` makes,
and the DCrypt side still builds and passes both suites after the `out_slot` change.

Things most likely to need a second pass, in order: `/W4 /WX` is on for EFI and the library has
only been through `/W3`; `dc_host_efi.c`'s `AllocatePool` wrapper changes alignment by
`sizeof(UINTN)`, which matters if anything assumes 16-byte alignment for an `xts_key`; and
image size, which nobody has measured.

## Step 8 — ImBox  ✅ done

### What was planned

`CCryptoIO` writes a `dc_dev` over `CAbstractIO` and its four statics become library calls:

| ImBox | Becomes |
|---|---|
| `CCryptoIO::VerifyPassword` | `cp_decrypt_header` on the read-back blob |
| `CCryptoIO::ChangePassword` | `dc_change_slot_pass` / `dc_update_key_slots` + `io_write_header` |
| `CCryptoIO::BackupHeader` | `dev->rw` read + `cp_encrypt_header` |
| `CCryptoIO::RestoreHeader` | `cp_decrypt_header` to verify, then `io_write_header` |
| `CCryptoIO::DiskRead` / `DiskWrite` | `dc_mount_io` |

ImBox is a separate product, so breakage is contained — but it goes last because it is the
only consumer whose requirements might force a library change, and by step 8 the other three
are settled.

### What was done, and why it is narrower

The table above is a rewrite of `CryptoIO.cpp`, not a port of `dc/`. It is still the right end
state and it is still outstanding — see *Deferred* — but it is a separate change from removing
the fourth copy of the code, and doing both at once would have made a behaviour regression
indistinguishable from a porting mistake.

So step 8 did what steps 6 and 7 did: `dc/dc_header.c` became an adapter over `volume_lib`,
keeping ImBox's exact names and signatures. `CryptoIO.cpp` changed in three places, none of
them behavioural — its include paths, and the signatures of `secure_alloc`/`secure_free`,
which were `PVOID(SIZE_T)` against `crypto_lib/cl_alloc.h`'s `void*(unsigned long)`: the same
`extern "C"` symbol declared two ways, so on x64 the callee read 64 bits of a register the
caller had filled 32 of.

ImBox reaches the library through three junctions, created by `ImBox/dc/setup_links.cmd`:
`dc/crypto_lib`, `dc/volume_lib` and `dc/_include_`, each pointing at the directory of that
name under `DCrypt`. `DCrypt/include` is deliberately not among them — the only thing ImBox
wanted from it was `dcconst.h`, for `ST_OK`, and `volume_lib/dc_status.h` exists to supply the
status codes where `dcconst.h` is out of reach, which is the same arrangement DCS uses.

`dc/include/defines.h` stays ImBox's — it is the host adaptation and declares the four hooks
`CryptoIO.cpp` defines. `volume_header.h` does not: it describes the on-disk format, and
there is no version of that which is ImBox's.

**ARM64 changed tier.** ImBox compiled `crypto_small` there — the portable C fallback — because
`crypto_fast` had no ARM64 path when the fork was taken. `crypto_lib` has one, so ARM64 now
runs crypto-extension AES and NEON Serpent. x64 gains the AVX2 Serpent and Argon2 cores.

**Win32 is not covered.** `crypto_lib` is x64 and ARM64 only, and `MajorPrivacy.sln` has no
`.Build.0` entry for ImBox on x86, so nothing was built for it before either. The
configuration is left as it was rather than wired to something that does not exist.

### The one thing the fork had and the library did not

`dc_derive_key_um` carried a branch for Argon2 cost values in 1001..1100 — "compatability for
0.99.7", where the cost was encoded as `1000 + cost` and expanded with a different formula.
Nothing in DiskCryptor has it. Dropping it would have stopped those containers opening, and
not visibly: the header would simply fail to decrypt and the user would be told the password
was wrong.

It could not live in ImBox's adapter, because `cp_decrypt_header` drives the KDF search inside
the library and an adapter never sees those calls. So it is in `crypto_head.c`. Purely
additive: `argon2_mk_params` rejects anything above 10, so `dc_derive_key` returned 0 for that
whole range before — no derivation that works today can change, only ones that fail can start
working.

Two other differences went the other way, and the library's version won: the fork's
`calculate_header_crc_um` masked `footer_cnt` to five unsigned bits where the format defines it
as signed, and its `KDF_NONE` path copied the raw key and then fell through to `return 0`.
Neither affects an existing container — ImBox never writes a non-zero `footer_cnt`, and the
raw-key path never worked.

### A short buffer is not an error

The adapter was copied from dcapi's, and carried over a check that does not belong in
ImBox: `if (enc_len < head_len) return ST_INV_VOLUME`.

It is right in dcapi, where every caller already holds the whole header — the GUI reads
`DC_AREA_MAX_SIZE` from the driver, `dc_load_header_file` reads the file — so a short buffer
there means a truncated backup. It is wrong in ImBox, where `CCryptoIO` reads
`cp_get_min_header_len()` bytes precisely because `head_len` lives *inside* the header and
cannot be known before the base is open. ImBox writes 64 KiB headers, so that is every volume
it creates: 2 KiB read, 65536 in `head_len`. They stopped opening, and stopped as
`ST_INV_VOLUME`, which the callers report as a wrong password.

The fix decrypts as much as was actually read and reports the real length in `out_len`, so a
caller can come back for the rest — the `goto redo` `ChangePassword` and `BackupHeader`
already have. Two things had to come with it:

- **whole sectors only.** `xts_decrypt` counts down by `XTS_SECTOR_SIZE` (`while (len -=
  XTS_SECTOR_SIZE)`), so a partial sector underflows the loop. A read that stops mid-sector
  leaves that sector for the re-read.
- **key slots only when they are all there.** `cp_copy_keylots` validates the slot area
  against `head_len`, not against what was read, so on a short buffer it would copy past the
  end. `cp_get_min_header_len` already sizes the first read to cover the slots whenever the
  password names one, so the guard costs nothing in practice.

`ImBox/dc/tests/` has the round-trip: build a header, seal it, open it from prefixes of
decreasing length, check that `key_1`, `alg_1` and `head_len` survive. Verified to fail with
the old check in place and pass with it gone.

The retired tree and the full list of drift are in `ImBox/dc/!old/README.md`.

---

## Keyfile mixing — done

Not part of the original eight steps; it came up afterwards for the same reason
everything else did. `dcapi/keyfiles.c` and `DcsPkg/…/dc_keyfiles.c` each
implemented the whole scheme, and the two had to agree exactly or a volume
unlocked with keyfiles in Windows would not unlock in the bootloader.

`volume_lib/keyfile_mix.c` now has both schemes — v1 additive and v2 canonical —
under `cp_kf_*`. What stayed behind is the part that cannot move: dcapi walks
directories with `FindFirstFile`, DCS loads through `FileLoad`, and each turns a
path into bytes its own way before handing over a hash. Their exported names and
signatures are unchanged, so `gui/`, `dccon/` and `DcsDiskCryptor.c` are
untouched; DCS's wrappers also translate `ST_*` into `EFI_STATUS`.

`dc_kf_mixer` had been declared twice, identically apart from the initial capacity
(16 in dcapi, 8 in DCS — allocation growth only, never the result). It is the
library's now, which is why `gui` and `dccon` gained `volume_lib` on their include
path: they take `dc_kf_mixer` through `dcapi/include/keyfiles.h`.

## Build tiers

Same mechanism as `crypto_lib`'s `CL_ENABLE_AVX2`:

| Tier | Define | Contents | UM | KM | EFI |
|---|---|---|---|---|---|
| Core | always | `crypto_head.c`, the read half of `header_io.c`, `dc_mount_io` | ● | ● | ● |
| Admin | `DCV_ENABLE_ADMIN` | `io_write_header`, `volume_head.c`, the step-4 editing additions | ● | ● | optional |
| Edit | implied by Admin && !kernel | slot and extended-header composition | ● | — | — |

The kernel gets Admin but not Edit: it applies a complete header handed down from user mode
via `io_write_header`'s `HF_UPDATE_*` flags, and never composes one. That is already how the
driver works — `init_header_v2` is the only exception, and it is called from `enc_dec.c`
during volume creation, so it stays kernel-side for now rather than being forced out.

| Project | Toolset | Platforms |
|---|---|---|
| `volume_lib.vcxproj` | v143 | x64, ARM64 |
| `volume_lib_km.vcxproj` | WindowsKernelModeDriver10.0 | x64, ARM64 |
| `volume_lib_efi.vcxproj` | EDK2 / `.inf` | x64 |

## Deferred

Not in the eight steps, on purpose:

- **`readwrite.c`.** Its region routing (`io_async_make_chunk`, `io_make_read_chunk_at_offset`)
  duplicates what `dc_mount_io` does, but it is welded to IRPs, chunking and the
  `tmp_size` snapshotting that lets transcryption run concurrently with I/O. Converging it
  onto the library's `dc_mount_io` is a real refactor of the driver's hottest and most
  concurrency-sensitive path, for no functional gain. Worth doing eventually, on its own,
  with its own test pass.
- **`enc_dec.c`.** Same argument, more so.
- **Reconciling the backup-header geometry.** DCS computes the backup redirection as
  `stor_off + stor_len - head_len + offset`; the driver uses `tail_off + offset` where
  `tail_off = stor_off + stor_len - tail_len`, keeping `tail_len` separate because the two
  "may transiently differ" (its own comment). DCS also re-enters `dc_mount_io` for redirected
  ranges, mapping them twice, where the driver maps once. Since `dc_mount_io` is being lifted
  from DCS in step 4 and the driver keeps its own path, this does not block anything — but it
  means the two implementations still disagree, and that should be settled before
  `readwrite.c` is ever converged onto the library.

## Verification

Header-format bugs are silent and permanent, so each step gets checked before the next:

- **Steps 1–2** are behaviour-preserving by construction. The check is the driver's existing
  test pass plus a mount/unmount/backup/restore/change-password cycle on a scratch volume.
- **Step 3** is where a differential harness pays for itself, the way `crypto_lib/tests` did:
  build the pre-move driver sources and the library side by side, run both over the same
  headers, slots, KDFs, cipher combinations and geometries, and compare byte for byte. The
  harness structure in `crypto_lib/tests` — `xts_equiv` for a readable per-case diff,
  `xts_bitexact` for the proof — transfers directly.
- **Steps 6–8** each get a round-trip test against a volume created by the *driver*, which is
  the compatibility property that actually matters: a header written by any consumer must
  still mount.

## Open questions

1. ~~`dc_dev.flags`~~ — settled: `dc_dev` carries two derived `opt` bits instead of the
   driver's `F_*` word. See "`opt`, not `flags`" above.
2. ~~`dc_change_pass_bak`'s wipe callback~~ — settled: `dc_wipe_process` becomes `host->wipe`
   and `wipe_ctx` stays the opaque parameter it already is in the driver.
3. **`init_header_v2`** stays kernel-side per the tier table, but it is the function that
   composes a header — the thing the kernel is otherwise not supposed to do. It is called
   during volume creation from `enc_dec.c`. Moving volume creation to user mode is a larger
   change than this refactor should take on, so it stays; worth revisiting later.
4. ~~**Licence, for ImBox.**~~ — settled twice over. It was never a blocker: `ImBox.cpp` is
   GPL-3.0-or-later in its own right, which combines with GPL-3.0-only. And `volume_lib` is
   MIT now regardless — the ~44 lines that still carried ntldr's expression were rewritten
   against 1.1.846.118, with the on-disk bytes held fixed by `tests/golden_test.c`. See
   `PROVENANCE.md`.
5. **Library name.** The directory is `volume_lib`; the functions keep their `cp_` / `io_` /
   `dc_` prefixes. That is inconsistent but it is what "minimise driver change" buys, and
   renaming later is mechanical.
