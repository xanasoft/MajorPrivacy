/*
    *
    * DiskCryptor - open source partition encryption tool
	* Copyright (c) 2026
	* DavidXanatos <info@diskcryptor.org>
    *

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <windows.h>
#include <stdio.h>
#include "dcconst.h"
#include "iso_fs.h"
#include "fs_sup.h"
#include "misc.h"

#define ISO_SECTOR_SIZE 2048
#define ISO_SYSTEM_AREA_SECTORS 16
#define MAX_DIRS 256
#define MAX_FILES 256
#define MAX_PATH_LEN 256

/* El Torito Section Header Entry */
struct iso_section_header {
	char header_indicator[ISODCL(1, 1)];
	char platform_id[ISODCL(2, 2)];
	char num_entries[ISODCL(3, 4)];
	char id_string[ISODCL(5, 32)];
};

/* El Torito Section Entry (same as initial entry) */
struct iso_section_entry {
	char boot_indicator[ISODCL(1, 1)];
	char media_type[ISODCL(2, 2)];
	char load_segment[ISODCL(3, 4)];
	char system_type[ISODCL(5, 5)];
	char unused[ISODCL(6, 6)];
	char sector_count[ISODCL(7, 8)];
	char load_rba[ISODCL(9, 12)];
	char selection_criteria[ISODCL(13, 13)];
	char unused2[ISODCL(14, 32)];
};

/* Directory entry in our internal tree */
typedef struct _dir_entry {
	char name[64];
	int parent_index;
	int level;
	u32 sector;
	u32 size;
	int first_child;
	int next_sibling;
} dir_entry;

/* File entry in our internal tree */
typedef struct _file_entry {
	char name[64];
	int dir_index;
	u32 sector;
	u32 size;
	const void *data;
} file_entry;

/* Helper to write 32-bit value in both little and big endian (733 format) */
static void write_733(char *dest, u32 value)
{
	p32(dest)[0] = value;
	p32(dest)[1] = BE32(value);
}

/* Helper to write 16-bit value in both little and big endian (723 format) */
static void write_723(char *dest, u16 value)
{
	p16(dest)[0] = value;
	p16(dest)[1] = BE16(value);
}

/* Helper to pad string with spaces */
static void write_padded_string(char *dest, const char *src, size_t len)
{
	size_t i;
	size_t src_len = src ? strlen(src) : 0;

	for (i = 0; i < len; i++) {
		dest[i] = (i < src_len) ? src[i] : ' ';
	}
}

/* Convert string to uppercase ISO format */
static void to_iso_name(const char *src, char *dest, size_t max_len)
{
	size_t i, len = 0;

	for (i = 0; src[i] && len < max_len - 3; i++) {
		char c = src[i];
		if (c >= 'a' && c <= 'z') {
			c = c - 'a' + 'A';
		}
		dest[len++] = c;
	}
	dest[len] = '\0';
}

/* Helper to write a directory record with explicit name length */
static size_t write_dir_record_ex(
	char *dest,
	const char *name,
	size_t name_len,
	u32 extent,
	u32 size,
	u8 flags
)
{
	u8 *ptr = (u8 *)dest;
	size_t base_size = 33;
	size_t record_len = base_size + name_len;
	size_t pos = 0;

	if (record_len & 1) {
		record_len++;
	}

	memset(dest, 0, record_len);

	ptr[pos++] = (u8)record_len;
	ptr[pos++] = 0;

	p32(ptr + pos)[0] = extent;
	p32(ptr + pos)[1] = BE32(extent);
	pos += 8;

	p32(ptr + pos)[0] = size;
	p32(ptr + pos)[1] = BE32(size);
	pos += 8;

	ptr[pos++] = 70;
	ptr[pos++] = 1;
	ptr[pos++] = 1;
	ptr[pos++] = 0;
	ptr[pos++] = 0;
	ptr[pos++] = 0;
	ptr[pos++] = 0;

	ptr[pos++] = flags;
	ptr[pos++] = 0;
	ptr[pos++] = 0;

	p16(ptr + pos)[0] = 1;
	p16(ptr + pos)[1] = BE16(1);
	pos += 4;

	ptr[pos++] = (u8)name_len;

	if (name_len > 0) {
		mincpy(ptr + pos, name, name_len);
	}

	return record_len;
}

/* Parse path and find/create directory */
static int find_or_create_dir(dir_entry *dirs, int *dir_count, const wchar_t *path, int *level)
{
	wchar_t path_copy[MAX_PATH_LEN];
	wchar_t *token, *next;
	int current_dir = 0;
	int current_level = 0;

	wcsncpy(path_copy, path, MAX_PATH_LEN - 1);
	path_copy[MAX_PATH_LEN - 1] = 0;

	token = path_copy;

	while (*token == L'\\' || *token == L'/') token++;

	while (*token) {
		char dir_name[64];
		size_t len = 0;
		int found = -1;
		int child;

		next = token;
		while (*next && *next != L'\\' && *next != L'/' && len < 63) {
			char c = (char)*next;
			if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
			dir_name[len++] = c;
			next++;
		}
		dir_name[len] = '\0';

		if (len == 0) break;

		if (*next == L'\\' || *next == L'/') {
			*next = 0;
			next++;
			while (*next == L'\\' || *next == L'/') next++;
		} else {
			break;
		}

		child = dirs[current_dir].first_child;
		while (child != -1) {
			if (strcmp(dirs[child].name, dir_name) == 0) {
				found = child;
				break;
			}
			child = dirs[child].next_sibling;
		}

		if (found == -1) {
			if (*dir_count >= MAX_DIRS) return -1;

			found = *dir_count;
			(*dir_count)++;

			strcpy(dirs[found].name, dir_name);
			dirs[found].parent_index = current_dir;
			dirs[found].level = current_level + 1;
			dirs[found].first_child = -1;
			dirs[found].next_sibling = dirs[current_dir].first_child;
			dirs[current_dir].first_child = found;
		}

		current_dir = found;
		current_level++;
		token = next;
	}

	*level = current_level;
	return current_dir;
}

/* Extract filename from path */
static void extract_filename_from_path(const wchar_t *path, char *out_name, size_t max_len)
{
	const wchar_t *filename = path;
	const wchar_t *p;
	size_t i, len = 0;

	for (p = path; *p; p++) {
		if (*p == L'\\' || *p == L'/') {
			filename = p + 1;
		}
	}

	for (i = 0; filename[i] && len < max_len - 3; i++) {
		char c = (char)filename[i];
		if (c >= 'a' && c <= 'z') {
			c = c - 'a' + 'A';
		}
		out_name[len++] = c;
	}

	if (len < max_len - 2) {
		out_name[len++] = ';';
		out_name[len++] = '1';
	}
	out_name[len] = '\0';
}

/* Build path table */
static void build_path_table(u8 *isobuf, dir_entry *dirs, int dir_count, u32 path_table_sector)
{
	u8 *pt = isobuf + (path_table_sector * ISO_SECTOR_SIZE);
	size_t offset = 0;
	int i;

	memset(pt, 0, ISO_SECTOR_SIZE * 2);

	for (i = 0; i < dir_count; i++) {
		u8 name_len = (u8)strlen(dirs[i].name);
		u16 parent_num = (i == 0) ? 1 : (u16)(dirs[i].parent_index + 1);

		pt[offset++] = name_len;
		pt[offset++] = 0;

		p32(pt + offset)[0] = dirs[i].sector;
		offset += 4;

		p16(pt + offset)[0] = parent_num;
		offset += 2;

		if (name_len > 0) {
			mincpy(pt + offset, dirs[i].name, name_len);
			offset += name_len;
		}

		if (offset & 1) offset++;
	}
}

/* Build directory content */
static size_t build_directory(
	char *dir_buf,
	dir_entry *dirs,
	int dir_index,
	file_entry *files,
	int file_count
)
{
	size_t offset = 0;
	int child;
	int i;

	offset += write_dir_record_ex(
		dir_buf + offset,
		"\x00",
		1,
		dirs[dir_index].sector,
		dirs[dir_index].size,
		0x02
	);

	if (dir_index == 0) {
		offset += write_dir_record_ex(
			dir_buf + offset,
			"\x01",
			1,
			dirs[dir_index].sector,
			dirs[dir_index].size,
			0x02
		);
	} else {
		offset += write_dir_record_ex(
			dir_buf + offset,
			"\x01",
			1,
			dirs[dirs[dir_index].parent_index].sector,
			dirs[dirs[dir_index].parent_index].size,
			0x02
		);
	}

	child = dirs[dir_index].first_child;
	while (child != -1) {
		offset += write_dir_record_ex(
			dir_buf + offset,
			dirs[child].name,
			strlen(dirs[child].name),
			dirs[child].sector,
			dirs[child].size,
			0x02
		);
		child = dirs[child].next_sibling;
	}

	for (i = 0; i < file_count; i++) {
		if (files[i].dir_index == dir_index) {
			offset += write_dir_record_ex(
				dir_buf + offset,
				files[i].name,
				strlen(files[i].name),
				files[i].sector,
				files[i].size,
				0x00
			);
		}
	}

	return offset;
}

/*
 * Creates an ISO 9660 filesystem image.
 *
 * If boot_file is provided, creates an El Torito bootable ISO.
 * The boot_file must be present in the files array.
 *
 * Files are written in the order they appear in the array, making it
 * easy to modify the last file by editing the end of the ISO file.
 */
int create_iso_image(const char* label, const file_entry_t* files, size_t count, const file_entry_t* boot_file, void **out_data, size_t *out_size)
{
	u8 *isobuf = NULL;
	dir_entry dirs[MAX_DIRS];
	file_entry file_entries[MAX_FILES];
	int dir_count = 1;
	int file_count = 0;
	u32 current_sector;
	u32 volume_sectors;
	u32 path_table_sector;
	u32 boot_catalog_sector = 0;
	int boot_file_index = -1;
	size_t total_size;
	size_t i;
	int resl = ST_ERROR;
	struct iso_primary_descriptor *pd, *bd, *td;
	u8 *root_rec;

	if (!out_data || !out_size || !files || count == 0) {
		return ST_ERROR;
	}

	memset(dirs, 0, sizeof(dirs));
	memset(file_entries, 0, sizeof(file_entries));

	dirs[0].name[0] = '\0';
	dirs[0].parent_index = 0;
	dirs[0].level = 0;
	dirs[0].first_child = -1;
	dirs[0].next_sibling = -1;

	for (i = 0; i < count && i < MAX_FILES; i++) {
		int dir_idx;
		int level = 0;

		dir_idx = find_or_create_dir(dirs, &dir_count, files[i].path, &level);
		if (dir_idx < 0) {
			return ST_ERROR;
		}

		extract_filename_from_path(files[i].path, file_entries[file_count].name, 64);
		file_entries[file_count].dir_index = dir_idx;
		file_entries[file_count].size = (u32)files[i].size;
		file_entries[file_count].data = files[i].data;
		file_count++;
	}

	/* Pre-calculate directory sizes to allocate sectors correctly */
	for (i = (size_t)dir_count; i > 0; i--) {
		char temp_buf[8192];
		size_t dir_size;

		/* Temporarily set size to estimate */
		dirs[i - 1].size = ISO_SECTOR_SIZE;
		dir_size = build_directory(temp_buf, dirs, (int)(i - 1), file_entries, file_count);
		dirs[i - 1].size = (u32)_align(dir_size, ISO_SECTOR_SIZE);
	}

	/* Find boot file if specified */
	if (boot_file) {
		for (i = 0; i < (size_t)file_count; i++) {
			if (files[i].data == boot_file->data && files[i].size == boot_file->size) {
				boot_file_index = (int)i;
				break;
			}
		}
	}

	/* Now allocate sectors based on actual sizes */
	current_sector = ISO_SYSTEM_AREA_SECTORS;
	current_sector += 3;
	path_table_sector = current_sector;
	current_sector += 2;

	/* Allocate boot catalog sector if bootable */
	if (boot_file_index >= 0) {
		boot_catalog_sector = current_sector;
		current_sector += 1;
	}

	for (i = 0; i < (size_t)dir_count; i++) {
		u32 dir_sectors = dirs[i].size / ISO_SECTOR_SIZE;
		dirs[i].sector = current_sector;
		current_sector += dir_sectors;
	}

	for (i = 0; i < (size_t)file_count; i++) {
		u32 file_sectors = (file_entries[i].size + ISO_SECTOR_SIZE - 1) / ISO_SECTOR_SIZE;
		file_entries[i].sector = current_sector;
		current_sector += file_sectors;
	}

	volume_sectors = current_sector;
	total_size = (size_t)volume_sectors * ISO_SECTOR_SIZE;

	do
	{
		if ((isobuf = calloc(1, total_size)) == NULL) {
			resl = ST_NOMEM;
			break;
		}

		/* Build descriptors with calculated sizes */
		pd = (struct iso_primary_descriptor *)addof(isobuf, ISO_SYSTEM_AREA_SECTORS * ISO_SECTOR_SIZE);
		bd = (struct iso_primary_descriptor *)addof(pd, ISO_SECTOR_SIZE);
		td = (struct iso_primary_descriptor *)addof(bd, ISO_SECTOR_SIZE);

		memset(pd, 0, sizeof(*pd));
		pd->type[0] = ISO_VD_PRIMARY;
		mincpy(pd->id, ISO_STANDARD_ID, sizeof(ISO_STANDARD_ID));
		pd->version[0] = 1;
		write_padded_string(pd->volume_id, label ? label : "DISK_IMAGE", 32);
		write_733(pd->volume_space_size, volume_sectors);
		write_723((char *)pd->volume_set_size, 1);
		write_723((char *)pd->volume_sequence_number, 1);
		write_723((char *)pd->logical_block_size, ISO_SECTOR_SIZE);

		{
			size_t pt_offset = 0;
			u8 temp_pt[4096];
			int j;

			memset(temp_pt, 0, sizeof(temp_pt));
			for (j = 0; j < dir_count; j++) {
				u8 name_len = (u8)strlen(dirs[j].name);
				temp_pt[pt_offset++] = name_len;
				pt_offset++;
				pt_offset += 4;
				pt_offset += 2;
				if (name_len > 0) pt_offset += name_len;
				if (pt_offset & 1) pt_offset++;
			}

			write_733(pd->path_table_size, (u32)pt_offset);
		}

		p32(pd->type_l_path_table)[0] = path_table_sector;
		pd->file_structure_version[0] = 1;

		/* Root directory record with calculated size */
		root_rec = (u8 *)pd->root_directory_record;
		write_dir_record_ex((char *)root_rec, "\x00", 1, dirs[0].sector, dirs[0].size, 0x02);

		memset(bd, 0, sizeof(*bd));
		bd->type[0] = ISO_VD_BOOT;
		mincpy(bd->id, ISO_STANDARD_ID, sizeof(ISO_STANDARD_ID));
		bd->version[0] = 1;
		mincpy(bd->system_id, "EL TORITO SPECIFICATION", 23);

		/* Point to boot catalog if bootable */
		/* Boot catalog pointer is at absolute bytes 71-74 */
		/* volume_id[31] + unused2[0-2] */
		if (boot_catalog_sector > 0) {
			bd->volume_id[31] = (u8)(boot_catalog_sector & 0xFF);
			bd->unused2[0] = (u8)((boot_catalog_sector >> 8) & 0xFF);
			bd->unused2[1] = (u8)((boot_catalog_sector >> 16) & 0xFF);
			bd->unused2[2] = (u8)((boot_catalog_sector >> 24) & 0xFF);
		}

		memset(td, 0, sizeof(*td));
		td->type[0] = ISO_VD_END;
		mincpy(td->id, ISO_STANDARD_ID, sizeof(ISO_STANDARD_ID));
		td->version[0] = 1;

		build_path_table(isobuf, dirs, dir_count, path_table_sector);

		/* Write El Torito boot catalog if bootable */
		if (boot_file_index >= 0 && boot_catalog_sector > 0) {
			struct iso_validation_entry *ve;
			struct iso_initial_entry *ie;
			struct iso_section_header *sh;
			struct iso_section_entry *se;
			u8 *boot_catalog = isobuf + (boot_catalog_sector * ISO_SECTOR_SIZE);
			u16 checksum;
			u16 *words;
			int j;

			memset(boot_catalog, 0, ISO_SECTOR_SIZE);

			/* Validation Entry - platform 0 (x86) */
			ve = (struct iso_validation_entry *)boot_catalog;
			ve->header_id[0] = 1;
			ve->platform_id[0] = 0; /* x86 platform */
			mincpy(ve->id_string, "", 24);
			ve->key_byte1[0] = 0x55;
			ve->key_byte2[0] = 0xAA;

			/* Calculate checksum - sum of all 16-bit words must be 0 */
			checksum = 0;
			words = (u16 *)ve;
			for (j = 0; j < 16; j++) {
				if (j != 14) { /* Skip checksum field itself */
					checksum += words[j];
				}
			}
			p16(ve->checksum)[0] = (u16)(0 - checksum);

			/* Initial/Default Entry - non-bootable (UEFI-only, no BIOS support) */
			ie = (struct iso_initial_entry *)(boot_catalog + 32);
			ie->boot_indicator[0] = 0x00; /* Non-bootable */
			ie->media_type[0] = 0; /* No emulation */
			/* Leave load_rba and sector_count as 0 */

			/* Section Header Entry for UEFI */
			sh = (struct iso_section_header *)(boot_catalog + 64);
			sh->header_indicator[0] = 0x91; /* Final header */
			sh->platform_id[0] = 0xEF; /* UEFI Platform ID */
			p16(sh->num_entries)[0] = 1; /* One entry in this section */
			mincpy(sh->id_string, "UEFI", 4);

			/* Section Entry for UEFI boot image */
			se = (struct iso_section_entry *)(boot_catalog + 96);
			se->boot_indicator[0] = 0x88; /* Bootable */
			se->media_type[0] = 0; /* No emulation */
			se->system_type[0] = 0; /* Unused */

			/* Load RBA points to the boot image in ISO sectors */
			p32(se->load_rba)[0] = file_entries[boot_file_index].sector;

			/* Sector count is in 512-byte virtual sectors */
			p16(se->sector_count)[0] = (u16)((file_entries[boot_file_index].size + 511) / 512);

			/* Selection criteria type: no selection criteria */
			se->selection_criteria[0] = 0;
		}

		/* Write directory records to their allocated sectors */
		for (i = 0; i < (size_t)dir_count; i++) {
			char *dir_buf = (char *)addof(isobuf, dirs[i].sector * ISO_SECTOR_SIZE);
			build_directory(dir_buf, dirs, (int)i, file_entries, file_count);
		}

		for (i = 0; i < (size_t)file_count; i++) {
			char *file_data_ptr = (char *)addof(isobuf, file_entries[i].sector * ISO_SECTOR_SIZE);
			if (file_entries[i].data && file_entries[i].size > 0) {
				mincpy(file_data_ptr, file_entries[i].data, file_entries[i].size);
			}
		}


		*out_data = isobuf;
		*out_size = total_size;
		resl = ST_OK;

	} while (0);

	if (resl != ST_OK && isobuf) {
		free(isobuf);
	}

	return resl;
}

/*
 * Extracts the volume label from an ISO 9660 image.
 *
 * Parameters:
 *   in_data    - Pointer to ISO image data
 *   in_size    - Size of ISO image data
 *   label      - Output pointer to label string (points into in_data)
 *   label_len  - Output length of label (excluding trailing spaces)
 *
 * Returns:
 *   ST_OK on success, ST_ERROR on failure
 */
int get_iso_label(char* in_data, size_t in_size, char** label, size_t* label_len)
{
	struct iso_primary_descriptor *pd;
	size_t i;

	if (!in_data || !label || !label_len) {
		return ST_ERROR;
	}

	/* Check minimum size for ISO header */
	if (in_size < (ISO_SYSTEM_AREA_SECTORS + 1) * ISO_SECTOR_SIZE) {
		return ST_ERROR;
	}

	/* Primary descriptor is at sector 16 */
	pd = (struct iso_primary_descriptor *)(in_data + ISO_SYSTEM_AREA_SECTORS * ISO_SECTOR_SIZE);

	/* Verify this is a primary descriptor */
	if (pd->type[0] != ISO_VD_PRIMARY) {
		return ST_ERROR;
	}

	/* Verify standard identifier */
	if (memcmp(pd->id, ISO_STANDARD_ID, 5) != 0) {
		return ST_ERROR;
	}

	/* Point to volume_id field (32 bytes at offset 40) */
	*label = pd->volume_id;

	/* Calculate length by finding last non-space character */
	*label_len = 32;
	for (i = 32; i > 0; i--) {
		if (pd->volume_id[i - 1] != ' ') {
			*label_len = i;
			break;
		}
	}

	return ST_OK;
}

/*
 * Finds a file in an ISO 9660 image by path and returns a pointer to its data.
 *
 * This function only works for non-fragmented files (single extent).
 * Fragmented files will return an error.
 *
 * Parameters:
 *   in_data   - Pointer to ISO image data
 *   in_size   - Size of ISO image data
 *   filePath  - Path to file (e.g., L"\\BOOT\\BOOTX64.EFI")
 *   file      - Output pointer to file data (points into in_data)
 *   file_size - Output file size
 *
 * Returns:
 *   ST_OK on success
 *   ST_ERROR on failure
 *   ST_ERROR if file is fragmented (cannot be accessed as contiguous memory)
 */
int get_iso_file(char* in_data, size_t in_size, const wchar_t* filePath, char** file, size_t* file_size)
{
	struct iso_primary_descriptor *pd;
	struct iso_directory_record *root_rec;
	char path_copy[MAX_PATH_LEN];
	const char *token, *next;
	char *dir_data;
	u32 dir_sector, dir_size;
	size_t i, len;

	if (!in_data || !filePath || !file || !file_size) {
		return ST_ERROR;
	}

	/* Check minimum size */
	if (in_size < (ISO_SYSTEM_AREA_SECTORS + 1) * ISO_SECTOR_SIZE) {
		return ST_ERROR;
	}

	/* Get primary descriptor */
	pd = (struct iso_primary_descriptor *)(in_data + ISO_SYSTEM_AREA_SECTORS * ISO_SECTOR_SIZE);

	/* Verify primary descriptor */
	if (pd->type[0] != ISO_VD_PRIMARY || memcmp(pd->id, ISO_STANDARD_ID, 5) != 0) {
		return ST_ERROR;
	}

	/* Get root directory record - it's at offset 156 in primary descriptor */
	root_rec = (struct iso_directory_record *)pd->root_directory_record;

	/* Read extent and size using byte offsets from the root record */
	dir_sector = p32((u8 *)root_rec + 2)[0];
	dir_size = p32((u8 *)root_rec + 10)[0];

	/* Convert wide char path to char and normalize to uppercase */
	len = 0;
	for (i = 0; filePath[i] && len < MAX_PATH_LEN - 1; i++) {
		char c = (char)filePath[i];
		if (c >= 'a' && c <= 'z') {
			c = c - 'a' + 'A';
		}
		path_copy[len++] = c;
	}
	path_copy[len] = '\0';

	token = path_copy;
	while (*token == '\\' || *token == '/') token++;

	/* Traverse directory structure */
	while (*token) {
		char component[64];
		size_t comp_len = 0;
		int found = 0;

		/* Validate directory sector and size */
		if ((size_t)dir_sector * ISO_SECTOR_SIZE + dir_size > in_size) {
			return ST_ERROR;
		}

		/* Point to current directory data */
		dir_data = in_data + dir_sector * ISO_SECTOR_SIZE;

		/* Extract next path component */
		next = token;
		while (*next && *next != '\\' && *next != '/' && comp_len < 63) {
			component[comp_len++] = *next++;
		}
		component[comp_len] = '\0';

		/* Check if this is the last component (file) or a directory */
		while (*next == '\\' || *next == '/') next++;
		int is_last = (*next == '\0');

		/* Search through directory entries */
		for (i = 0; i < dir_size; ) {
			u8 *rec_ptr = (u8 *)(dir_data + i);
			u8 rec_len, rec_name_len, rec_flags;
			u32 rec_extent, rec_size;
			char *rec_name;

			/* Read record length (byte 0) */
			rec_len = rec_ptr[0];

			/* Check for end of entries in current sector (padding) */
			if (rec_len == 0) {
				/* Skip to next sector boundary */
				size_t next_sector_offset = ((i / ISO_SECTOR_SIZE) + 1) * ISO_SECTOR_SIZE;
				if (next_sector_offset >= dir_size) break;
				i = next_sector_offset;
				continue;
			}

			/* Verify record length is valid */
			if (rec_len < 33 || i + rec_len > dir_size) {
				return ST_ERROR;
			}

			/* Read fields using direct byte offsets (ISO 9660 spec):
			 * Byte 0: length
			 * Byte 1: ext attr length
			 * Bytes 2-9: extent (733 format)
			 * Bytes 10-17: size (733 format)
			 * Bytes 18-24: date
			 * Byte 25: flags
			 * Byte 26: file unit size
			 * Byte 27: interleave
			 * Bytes 28-31: volume sequence number
			 * Byte 32: name length
			 * Byte 33+: name
			 */
			rec_extent = p32(rec_ptr + 2)[0];
			rec_size = p32(rec_ptr + 10)[0];
			rec_flags = rec_ptr[25];
			rec_name_len = rec_ptr[32];
			rec_name = (char *)(rec_ptr + 33);

			/* Skip "." and ".." entries */
			if (rec_name_len == 1 && (rec_name[0] == 0 || rec_name[0] == 1)) {
				i += rec_len;
				continue;
			}

			/* Compare name (ISO names include ";1" version suffix) */
			if (rec_name_len >= comp_len) {
				int match = 1;
				size_t j;

				for (j = 0; j < comp_len; j++) {
					if (rec_name[j] != component[j]) {
						match = 0;
						break;
					}
				}

				/* For files, allow ";1" version suffix */
				if (match && is_last) {
					if (rec_name_len == comp_len ||
					    (rec_name_len == comp_len + 2 &&
					     rec_name[comp_len] == ';' &&
					     rec_name[comp_len + 1] == '1')) {
						found = 1;
					}
				} else if (match && !is_last && rec_name_len == comp_len) {
					/* Directory name must match exactly */
					found = 1;
				}
			}

			if (found) {
				if (is_last) {
					/* This should be a file, not a directory */
					if (rec_flags & 0x02) {
						return ST_ERROR;
					}

					/* Check if file extends beyond ISO image */
					if ((size_t)rec_extent * ISO_SECTOR_SIZE + rec_size > in_size) {
						return ST_ERROR;
					}

					/* Check for fragmentation: file_unit_size and interleave at bytes 26, 27 */
					if (rec_ptr[26] != 0 || rec_ptr[27] != 0) {
						/* File is fragmented/interleaved */
						return ST_ERROR;
					}

					/* Return pointer to file data */
					*file = in_data + rec_extent * ISO_SECTOR_SIZE;
					*file_size = rec_size;
					return ST_OK;
				} else {
					/* This should be a directory */
					if (!(rec_flags & 0x02)) {
						return ST_ERROR;
					}

					/* Move to this directory */
					dir_sector = rec_extent;
					dir_size = rec_size;
					break;
				}
			}

			i += rec_len;
		}

		if (!found) {
			return ST_ERROR;
		}

		token = next;
	}

	/* Empty path or only slashes */
	return ST_ERROR;
}

/* Helper structure for building file list during extraction */
typedef struct {
	file_entry_t* entries;
	size_t count;
	size_t capacity;
	char* in_data;
	size_t in_size;
} extract_context_t;

/* Helper to add a file to the extraction context */
static int add_extracted_file(extract_context_t* ctx, const wchar_t* path, u32 extent, u32 size)
{
	file_entry_t* entry;

	/* Grow array if needed */
	if (ctx->count >= ctx->capacity) {
		size_t new_capacity = ctx->capacity == 0 ? 16 : ctx->capacity * 2;
		file_entry_t* new_entries = realloc(ctx->entries, new_capacity * sizeof(file_entry_t));
		if (!new_entries) {
			return ST_NOMEM;
		}
		ctx->entries = new_entries;
		ctx->capacity = new_capacity;
	}

	/* Add entry */
	entry = &ctx->entries[ctx->count];
	wcscpy(entry->path, path);
	entry->size = size;
	entry->data = ctx->in_data + extent * ISO_SECTOR_SIZE;
	ctx->count++;

	return ST_OK;
}

/* Recursive helper to traverse a directory and extract all files */
static int extract_directory(extract_context_t* ctx, u32 dir_sector, u32 dir_size, const wchar_t* dir_path)
{
	char* dir_data;
	size_t i;
	wchar_t current_path[MAX_PATH_LEN];
	size_t dir_path_len = wcslen(dir_path);

	/* Validate directory sector and size */
	if ((size_t)dir_sector * ISO_SECTOR_SIZE + dir_size > ctx->in_size) {
		return ST_ERROR;
	}

	dir_data = ctx->in_data + dir_sector * ISO_SECTOR_SIZE;

	/* Iterate through directory entries */
	for (i = 0; i < dir_size; ) {
		u8 *rec_ptr = (u8 *)(dir_data + i);
		u8 rec_len, rec_name_len, rec_flags;
		u32 rec_extent, rec_size;
		char *rec_name;
		wchar_t name_wide[64];
		size_t j;

		/* Read record length */
		rec_len = rec_ptr[0];

		/* Check for end of entries in current sector (padding) */
		if (rec_len == 0) {
			/* Skip to next sector boundary */
			size_t next_sector_offset = ((i / ISO_SECTOR_SIZE) + 1) * ISO_SECTOR_SIZE;
			if (next_sector_offset >= dir_size) break;
			i = next_sector_offset;
			continue;
		}

		/* Verify record length is valid */
		if (rec_len < 33 || i + rec_len > dir_size) {
			return ST_ERROR;
		}

		/* Read fields using direct byte offsets */
		rec_extent = p32(rec_ptr + 2)[0];
		rec_size = p32(rec_ptr + 10)[0];
		rec_flags = rec_ptr[25];
		rec_name_len = rec_ptr[32];
		rec_name = (char *)(rec_ptr + 33);

		/* Skip "." and ".." entries */
		if (rec_name_len == 1 && (rec_name[0] == 0 || rec_name[0] == 1)) {
			i += rec_len;
			continue;
		}

		/* Convert name to wide char, removing ";1" version suffix for files */
		for (j = 0; j < rec_name_len && j < 63; j++) {
			/* Stop at version separator for files */
			if (!(rec_flags & 0x02) && rec_name[j] == ';') {
				break;
			}
			name_wide[j] = (wchar_t)(unsigned char)rec_name[j];
		}
		name_wide[j] = L'\0';

		/* Build full path */
		if (dir_path_len == 0) {
			/* Root directory */
			swprintf_s(current_path, MAX_PATH_LEN, L"\\%s", name_wide);
		} else {
			swprintf_s(current_path, MAX_PATH_LEN, L"%s\\%s", dir_path, name_wide);
		}

		if (rec_flags & 0x02) {
			/* This is a subdirectory - recurse */
			int result = extract_directory(ctx, rec_extent, rec_size, current_path);
			if (result != ST_OK) {
				return result;
			}
		} else {
			/* This is a file - add it */
			int result = add_extracted_file(ctx, current_path, rec_extent, rec_size);
			if (result != ST_OK) {
				return result;
			}
		}

		i += rec_len;
	}

	return ST_OK;
}

/*
 * Extracts all files from an ISO 9660 image.
 *
 * This function traverses the entire ISO directory structure and builds
 * an array of file_entry_t containing all files with their full paths.
 *
 * Parameters:
 *   in_data - Pointer to ISO image data
 *   in_size - Size of ISO image data
 *   files   - Output pointer to allocated array of file_entry_t
 *   count   - Output number of files found
 *
 * Returns:
 *   ST_OK on success, ST_ERROR or ST_NOMEM on failure
 *
 * Note: Caller must free the returned array and each path string.
 */
int extract_iso_image(void *in_data, size_t in_size, file_entry_t** files, size_t* count)
{
	struct iso_primary_descriptor *pd;
	struct iso_directory_record *root_rec;
	extract_context_t ctx;
	u32 root_sector, root_size;
	int result;

	if (!in_data || !files || !count) {
		return ST_ERROR;
	}

	/* Check minimum size */
	if (in_size < (ISO_SYSTEM_AREA_SECTORS + 1) * ISO_SECTOR_SIZE) {
		return ST_ERROR;
	}

	/* Get primary descriptor */
	pd = (struct iso_primary_descriptor *)((char *)in_data + ISO_SYSTEM_AREA_SECTORS * ISO_SECTOR_SIZE);

	/* Verify primary descriptor */
	if (pd->type[0] != ISO_VD_PRIMARY || memcmp(pd->id, ISO_STANDARD_ID, 5) != 0) {
		return ST_ERROR;
	}

	/* Get root directory record */
	root_rec = (struct iso_directory_record *)pd->root_directory_record;
	root_sector = p32((u8 *)root_rec + 2)[0];
	root_size = p32((u8 *)root_rec + 10)[0];

	/* Initialize extraction context */
	memset(&ctx, 0, sizeof(ctx));
	ctx.in_data = (char *)in_data;
	ctx.in_size = in_size;

	/* Extract all files starting from root */
	result = extract_directory(&ctx, root_sector, root_size, L"");

	if (result != ST_OK) {
		/* Clean up on error */
		if (ctx.entries) {
			size_t i;
			for (i = 0; i < ctx.count; i++) {
				if (ctx.entries[i].path) {
					free((void *)ctx.entries[i].path);
				}
			}
			free(ctx.entries);
		}
		return result;
	}

	/* Return results */
	*files = ctx.entries;
	*count = ctx.count;

	return ST_OK;
}