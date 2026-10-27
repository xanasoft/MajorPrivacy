# Where this code came from

`volume_lib` is MIT. It did not start that way, and the reason it can be is
recorded here rather than assumed.

The library was extracted from DiskCryptor's driver, and the driver is GPL-3
with ntldr's copyright on much of it. So before the licence changed, every file
was compared against the last release he authored — **DiskCryptor
1.1.846.118** — function by function, to find what was still his expression
rather than a description of the same job.

## What the comparison found

`volume_lib` is about 1,570 lines of C across six files. Traceable ntldr
expression was roughly 44 lines of it, and almost all of it sat in one file.

Counting non-trivial lines of his that still appeared, ignoring whitespace:

| 1.1.846.118 | volume_lib | his lines | surviving | |
|---|---|---|---|---|
| `is_volume_header_correct` | same name | 8 | 7 | 88% — his function, two trivial edits |
| `io_write_header` | same name | 44 | 22 | his core, inside a 155-line function |
| `io_read_header` | same name | 20 | 10 | the allocate / read / decrypt / clean up skeleton |
| `cp_decrypt_header` | `cp_try_decrypt_header` | 26 | 4 | the cipher loop |
| `cp_set_header_key` | same name | 7 | 1 | `burn(dk, sizeof(dk));` |
| `dc_change_pass` | `dc_change_pass_bak` | 31 | 2 | nothing meaningful |
| `dc_backup_header` | `dc_update_backup` | 40 | 2 | nothing meaningful |

Fifteen of his comments also survived verbatim, fourteen of them in
`header_io.c` — "add volume header to random pool because RNG not have
sufficient entropy at boot time", "generate new salt", "restore original salt"
and so on. Comments are the clearest trace of copying there is, because unlike
code they are not constrained by the task.

Everything else had **no counterpart in his tree at all**: all of
`head_util.c`, `dc_host.c`, `dc_buffer.c`; `dc_change_slot_pass` and
`dc_update_key_slots`; `calculate_header_crc`, `get_ext_header` and
`io_read_header_full`; and ten functions in `crypto_head.c`, including the
whole KDF and key-slot machinery, which did not exist in 2010.

## What was done about it

The functions in that table were rewritten, and the fifteen comments replaced.
The rewrite changed expression only — not behaviour, not the on-disk format,
not a single byte a volume is sealed with.

That last claim is checked rather than asserted. `tests/golden_test.c` drives
every header entry point over a buffer-backed device with a deterministic RNG —
every cipher, both header versions, headers with and without key slots, 512
and 4096-byte sectors, and each of the partial-update flags — and folds what
comes out into two SHA-512s:

| | |
|---|---|
| `written to disk` | `cf783c634b236a6f0b86bc5f3353080062d19ae8d889ef90dfede54043c04bb1` |
| `everything` | `faa938d94e817414ac0a1dbd34dd5f4e4895db5d02e53b9bee6f8a5e892e9db2` |

The first covers only bytes that reach a disk, and is the compatibility
contract: if it moves, headers written by an older build stop matching what
this one produces. The second adds everything read back and every code
returned, so a fix to a read path moves it and must leave the first alone.

Before the rewrite, with one combined digest, the value was
`f2fd41d3faa999cefc362665bea012301d7e6c515959b19dfc9559c8d9a762c4`, and the
rewrite left it exactly there. A header that differs by one byte is a volume
that no longer opens, so "the tests still pass" was not a strong enough claim
to rely on.

### One thing the round-trip check then caught

The golden digest only says "the same as last time", so `golden_test.c` also
asserts the obvious thing it cannot: that a header written and read back is the
header that went in. It was not.

`io_read_header_full` decrypted the part of the header past `DC_AREA_SIZE`
from `buff` rather than `buff + DC_AREA_SIZE` — the base's ciphertext, run a
second time under the tail's tweak and written over the tail. Every header
longer than 2 KiB came back with rubbish past the base. Around 6,100 of 8,128
bytes differed on an 8 KiB header.

It had gone unnoticed because nothing that decides whether a volume mounts
lives out there: the base carries the keys and the version, and the key slots
are copied across raw a few lines later. The extended header is what was being
read as noise.

The fix is one line, and the `written to disk` digest is byte-identical either
side of it — measured, by reverting the fix and re-running. So no volume was
ever written wrongly, and none needs rewriting: only the reader was wrong.

## keyfile_mix.c, added afterwards

The keyfile mixing was consolidated into the library after the relicence, from
`dcapi/keyfiles.c` and DcsPkg's `dc_keyfiles.c` — the same scheme written twice,
same domain strings, same sort-then-deduplicate, differing only in how they
allocated and which sort they had available.

The same check was applied. ntldr's `keyfiles.c` in 1.1.846.118 contains exactly
two functions, `dc_add_single_kf` and `dc_add_keyfiles`, and both are about
reading files. The canonical v2 scheme — hash each keyfile, sort, deduplicate,
fold under `DC2-KFSET-v1`, mix with the password under `DC2-PW-KF-v1` — has no
ancestor there at all.

What does predate it is the v1 additive mixing, five lines inside his
`dc_add_single_kf` that add the digest into the password word by word. That is now
`cp_kf_mix_additive`, re-expressed; measured against his function, **zero** of his
non-trivial lines remain. The arithmetic is identical, wraparound included,
because every volume whose keyfiles were applied that way depends on it.

His file-reading code is untouched and still in `dcapi/keyfiles.c`, which is GPL-3
and still carries his copyright. Only the mixing moved.

`tests/keyfile_test.c` checks the move differentially rather than against a golden
digest: both originals are reproduced in the test and every scenario is run
through the original and the library and compared. Keyfiles feed the KDF, so a
difference is a volume that cannot be opened, reported as nothing more useful than
"wrong password".

## What is deliberately not claimed

The volume header *format* is DiskCryptor's design and is visible throughout —
the `dc_header` layout, which fields the CRC covers, the rule that the salt
stays in the clear because it derives the key that decrypts everything else.
A data format is not copyrightable expression, and sequences like "validate,
derive, decrypt" are dictated by the format rather than chosen. Those were not
treated as anything to rewrite, and could not have been without changing the
format itself.

`volume_lib` also still depends on DiskCryptor's headers — `volume_header.h`,
`dcconst.h`, `defines.h` — which live in the main tree under its licence. It
is MIT as a body of code, not as a self-contained distribution.

## The other direction

Several differences ran the other way, where the current code is ahead of his
and the comparison simply confirmed no shared text. `crypto_head.c` gained
Argon2id with ten cost levels, key slots with XOR-wrapped header keys, the
`KDF_ALL` / `KDF_DEFAULT` search, and an abort flag threaded through the long
derivations. `header_io.c` gained partial header updates, v2 headers of
arbitrary length, and the extended header. None of that has a 2010 ancestor.
