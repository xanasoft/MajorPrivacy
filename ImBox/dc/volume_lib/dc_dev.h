/*
    * DCrypt volume_lib - device description and host hooks
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * This is the only genuinely new interface in volume_lib. Everything else is
    * the driver's own code, moved, under its own names.
    *
    * Two things in the moved code cannot cross into a library: dev_hook, which
    * is full of PDEVICE_OBJECT / KEVENT / LIST_ENTRY, and the driver facilities
    * the moved code calls by name (mm_secure_alloc, cp_rand_bytes, DbgMsg,
    * dc_wipe_process). dc_dev replaces the first; dc_host replaces the rest.
    *
    *
    * WHY dc_dev IS PURE DATA
    *
    * dc_dev holds no function pointers, on purpose. It describes *what the
    * device is*, never *how to reach it* - the I/O callback is a separate
    * argument, and the wipe callback is a host hook.
    *
    * That keeps one door open. Today the driver projects its dev_hook into a
    * caller-owned dc_dev, which copies about fifty bytes onto the stack.
    * Tomorrow it could instead nest a dc_dev inside dev_hook and pass
    * &hook->dev, dropping both the copy and dc_dev_of() - and because dc_dev is
    * pure data, that switch needs no per-hook initialisation, no function
    * pointer to keep valid, and above all NO CHANGE TO THIS LIBRARY. A struct
    * carrying a callback could not be nested so cleanly: it would need setting
    * up at hook creation and would carry a redundant pointer in every
    * instance.
    *
    * To be clear about what that is worth: the copy itself is not the reason.
    * Fifty bytes against a header read that does a KDF and hits a disk is free.
    * The reason is that the driver can change its mind later without the
    * library, and every consumer of it, having to move.
    *
    *
    * WHY THE FIELDS ARE THE ONES THEY ARE
    *
    * The shape is decided by arithmetic. Moving the geometry fields out of
    * dev_hook into a library-owned struct would mean about 445 edits in the
    * driver - hook->head_len alone appears 109 times. Changing only the device
    * parameter of the functions that move costs about 40. So the fields stay
    * where they are and dc_dev is a projection of them.
    *
    * hook->flags is deliberately NOT one of them. It is the worst field in the
    * driver to move (224 reads, 57 writes) and it is mount state rather than
    * geometry. The moved code only ever asks two questions of it, so dc_dev
    * carries the two answers instead of the whole word - see dc_dev.opt. That
    * also keeps the driver's F_* namespace out of an interface that ImBox and
    * DCS have to fill in.
*/
#ifndef _DC_DEV_H_
#define _DC_DEV_H_

#include "defines.h"
#include "volume_header.h"
#include "xts_fast.h"

/*
 * va_list, for the host's log hook below.
 *
 * EDK2 has no C runtime and spells the type VA_LIST, which Base.h supplies
 * through defines.h above. Including <stdarg.h> there pulls vcruntime.h in
 * with it, which then redeclares wchar_t behind the firmware typedef.
 */
#if defined(_UEFI)
typedef VA_LIST va_list;
#else
#include <stdarg.h>
#endif

#if defined(_KERNEL_MODE) || defined(IS_DRIVER)
 #define DC_LIB_KERNEL 1
#else
 #define DC_LIB_KERNEL 0
#endif

/* ---------------------------------------------------------------------------
 * Device
 * ------------------------------------------------------------------------ */

/*
 * The two things the moved code needs to know about a volume's layout that are
 * not lengths. Derived by the caller; the driver computes them from its F_*
 * flags in dc_dev_of(), other hosts set them directly.
 */
#define DC_DEV_BACKUP_HEADER   0x01  /* a backup header lives at the device end.
                                        driver: hook->flags & F_HEAD_BACKUP */
#define DC_DEV_STORAGE_ON_END  0x02  /* the redirection area holds the partition's
                                        end sectors, so the backup header is
                                        redirected into it rather than written
                                        in place. driver: IS_STORAGE_ON_END(),
                                        i.e. F_ENABLED && !F_CDROM &&
                                        !F_PROTECT_DCSYS && !F_NO_REDIRECT */

/*
 * The subset of dev_hook that the moved functions read. In the driver this is
 * filled by dc_dev_of(hook, &dev) into a local; elsewhere the consumer fills
 * it directly.
 *
 * IMPORTANT: this projection is READ-ONLY. It works because no moved function
 * writes back to the hook - io_read_header and io_read_header_full return
 * through out-parameters, io_write_header writes nothing to the device struct,
 * and dc_update_backup writes nothing at all. Anything moved later
 * that needs to change device state must return the new value rather than
 * store it here, or the driver and the projection will silently disagree.
 */
typedef struct _dc_dev {
	/* Handed back to the rw callback untouched. dev_hook* in the driver, a
	   BlockIo in EFI, a CAbstractIO* in ImBox, a file handle or a buffer in
	   dcapi. Opaque to the library. */
	void     *ctx;

	u32       bps;          /* bytes per sector */
	u64       dsk_size;     /* full device size in bytes */

	u32       head_len;     /* header length, and offset to data */
	u32       tail_len;     /* backup header length; may differ from head_len */
	u32       stor_len;     /* redirection area length */

	/* Not read by the moved code; dc_mount_io needs it (step 4). */
	u64       stor_off;     /* redirection area offset, 0 if unused */

	u32       opt;          /* DC_DEV_* above */

} dc_dev;

/*
 * There is deliberately no hdr_key here. dc_update_backup carries a comment
 * saying it falls back to hook->hdr_key, but the code does not - it validates
 * the caller's bak_key and fails if that is missing. Keeping a field for a
 * fallback that does not exist would have put 31 more accesses in the way of
 * ever nesting this struct, for nothing.
 */

/*
 * Block I/O, passed alongside a dc_dev rather than inside it.
 *
 * Exactly io_hook_rw's signature with dev_hook* replaced by void*, so the
 * driver supplies it as a one-line thunk and io_hook_rw itself - and all 34 of
 * its existing call sites - stay untouched. Returns ST_OK or an ST_* error.
 * offset and length are byte quantities, sector-aligned when the library calls.
 */
typedef int (*dc_rw_fn)(void *ctx, void *buff, u32 length, u64 offset, int is_read);

/* ---------------------------------------------------------------------------
 * Host
 * ------------------------------------------------------------------------ */

/*
 * The driver facilities the moved code calls by name. Installed once.
 *
 * secure_alloc must return memory fit to hold key material - locked against
 * paging where the platform can do that - and secure_free must wipe it. The
 * moved code assumes both, because mm_secure_alloc and secure_alloc both do.
 */
typedef struct _dc_host {
	void *(*secure_alloc)(size_t size);
	void  (*secure_free)(void *p);

	void  (*rand_bytes)(void *buff, size_t size);

	/*
	 * Optional. header_io.c feeds the old header in before generating a new
	 * salt, because the driver's pool has little entropy at boot time. NULL is
	 * fine for hosts whose RNG does not want the contribution.
	 */
	void  (*rand_add_seed)(const void *buff, size_t size);

	/*
	 * Optional diagnostic sink; replaces DbgMsg. May be NULL.
	 *
	 * Takes a va_list rather than being variadic: the kernel has no usable
	 * vsnprintf at arbitrary IRQL, and vDbgPrintEx consumes a va_list directly,
	 * so forwarding one costs the driver nothing. A host that wants the text
	 * formats it itself.
	 */
	void  (*log)(const char *fmt, va_list args);

	/*
	 * Optional. Replaces dc_wipe_process. Only dc_change_pass_bak uses it, to
	 * wipe the old backup header before overwriting; NULL disables wiping.
	 * wipe_ctx stays an opaque parameter on that function, exactly as it is a
	 * parameter in the driver today.
	 */
	void  (*wipe)(void *wipe_ctx, u64 offset, int size);

} dc_host;

/*
 * Install the host. Call once before anything else. A second call with an
 * identical host is a no-op; with a different one it fails, so two components
 * in one process cannot quietly fight over the allocator.
 *
 * This does NOT call xts_init(). crypto_lib is the caller's to initialise,
 * because only the caller knows when: the driver takes its hw_crypt flag from
 * configuration that is not loaded yet at the point the host has to be
 * installed. Folding xts_init() in here would invent an ordering constraint
 * that does not otherwise exist.
 */
int dc_lib_init(const dc_host *host);

/* ---------------------------------------------------------------------------
 * Driver-side glue
 * ------------------------------------------------------------------------ */
#if DC_LIB_KERNEL
/*
 * Declared here, defined in the driver (devhook.c): projects the live hook
 * fields into a dc_dev the caller owns.
 *
 * The dc_dev belongs on the caller's stack, not in dev_hook. A member would be
 * a second copy of state the hook already holds, shared by every caller and
 * outliving every one of them - and since the projection is refreshed rather
 * than maintained, whether it was current would depend on who called last.
 * A local is private, obviously short-lived, and costs one struct of stack.
 *
 * Fill it once per operation and pass &dev from there on. The exception is
 * code that changes the layout as it goes - dc_apply_layout resizes the header
 * and moves the redirection area between writes - which must refill before
 * each use. Those sites say so.
 */
struct _dev_hook;
void dc_dev_of(struct _dev_hook *hook, dc_dev *dev);

/* The io_hook_rw thunk. One line in device_io.c. */
int dc_hook_rw(void *ctx, void *buff, u32 length, u64 offset, int is_read);
#endif

/* ---------------------------------------------------------------------------
 * Consumer-side helpers
 * ------------------------------------------------------------------------ */

/*
 * A dc_dev plus rw backed by a plain memory buffer. dcapi needs this for
 * dc_decrypt_header, which works on a header blob already in memory rather
 * than on a device. Reads and writes outside the buffer fail with ST_RW_ERR.
 *
 * The caller owns the descriptor - declare one next to the dc_dev, typically
 * both on the stack. It is not held internally, so any number of buffer
 * devices can be open at once and from any thread.
 */
typedef struct _dc_buffer {
	u8  *data;
	u32  size;
} dc_buffer;

int      dc_dev_from_buffer(dc_dev *dev, dc_buffer *desc, void *buff, u32 size, u32 bps);
dc_rw_fn dc_buffer_rw(void);

#endif /* _DC_DEV_H_ */
