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
#include "fs_sup.h"
#include "misc.h"

#define FAT_SECTOR_SIZE 512
#define FAT_DIR_ENTRY_SIZE 32

#pragma pack(push, 1)

/* FAT12/16 Boot Parameter Block */
typedef struct _fat_boot_sector {
	u8  jmp_boot[3];
	u8  oem_name[8];
	u16 bytes_per_sector;
	u8  sectors_per_cluster;
	u16 reserved_sectors;
	u8  num_fats;
	u16 root_entries;
	u16 total_sectors_16;
	u8  media_type;
	u16 fat_size_16;
	u16 sectors_per_track;
	u16 num_heads;
	u32 hidden_sectors;
	u32 total_sectors_32;
	u8  drive_number;
	u8  reserved1;
	u8  boot_signature;
	u32 volume_id;
	u8  volume_label[11];
	u8  fs_type[8];
	u8  boot_code[448];
	u16 boot_sector_sig;
} fat_boot_sector;

/* FAT directory entry */
typedef struct _fat_dir_entry {
	u8  name[11];
	u8  attr;
	u8  nt_reserved;
	u8  create_time_tenth;
	u16 create_time;
	u16 create_date;
	u16 last_access_date;
	u16 first_cluster_hi;
	u16 write_time;
	u16 write_date;
	u16 first_cluster_lo;
	u32 file_size;
} fat_dir_entry;

/* FAT Long File Name entry */
typedef struct _fat_lfn_entry {
	u8  sequence;
	u16 name1[5];
	u8  attr;
	u8  type;
	u8  checksum;
	u16 name2[6];
	u16 first_cluster_lo;
	u16 name3[2];
} fat_lfn_entry;

#pragma pack(pop)

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LONG_NAME 0x0F

#define LFN_LAST_ENTRY 0x40


/* Calculate checksum for 8.3 filename */
static u8 lfn_checksum(const u8 *name)
{
	u8 sum = 0;
	int i;

	for (i = 0; i < 11; i++) {
		sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + name[i];
	}

	return sum;
}

/* Generate 8.3 short name with numeric tail */
static void generate_short_name(const char *long_name, u8 *short_name, int sequence)
{
	size_t i, j, name_len = 0, ext_len = 0;
	const char *ext = NULL;
	const char *last_dot = NULL;
	char tail[8];

	memset(short_name, ' ', 11);

	/* Find last dot for extension */
	for (i = 0; long_name[i]; i++) {
		if (long_name[i] == '.') {
			last_dot = &long_name[i];
		}
	}

	if (last_dot) {
		ext = last_dot + 1;
	}

	/* Build base name (up to 6 chars to leave room for ~1) */
	for (i = 0; long_name[i] && &long_name[i] != last_dot && name_len < 6; i++) {
		char c = long_name[i];
		if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
		if (c != ' ' && c != '.') {
			short_name[name_len++] = (u8)c;
		}
	}

	/* Add numeric tail ~1, ~2, etc. */
	if (sequence > 0) {
		_snprintf(tail, sizeof(tail), "~%d", sequence);
		for (j = 0; tail[j] && name_len < 8; j++) {
			short_name[name_len++] = (u8)tail[j];
		}
	}

	/* Add extension */
	if (ext) {
		for (i = 0; ext[i] && ext_len < 3; i++) {
			char c = ext[i];
			if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
			if (c != ' ' && c != '.') {
				short_name[8 + ext_len++] = (u8)c;
			}
		}
	}
}

/* Convert filename to FAT 8.3 format (simple version without LFN) */
static void to_fat_name(const char *src, u8 *dest)
{
	generate_short_name(src, dest, 1);
}

/* Calculate number of LFN entries needed for a name */
static int calculate_lfn_entries(const char *long_name)
{
	size_t name_len = strlen(long_name);
	return (int)((name_len + 12) / 13);
}

/* Write LFN entries and return number of entries written */
static int write_lfn_entries(u8 *dir_ptr, const char *long_name, u8 checksum)
{
	size_t name_len = strlen(long_name);
	int num_entries = (int)((name_len + 12) / 13);
	int entry_idx, char_idx;
	u16 uni_name[256];
	size_t i;

	/* Convert to Unicode */
	for (i = 0; i <= name_len && i < 255; i++) {
		uni_name[i] = (u16)(u8)long_name[i];
	}
	uni_name[i] = 0;

	/* Pad with 0xFFFF */
	for (; i < 256; i++) {
		uni_name[i] = 0xFFFF;
	}

	/* Write LFN entries in reverse order */
	for (entry_idx = num_entries - 1; entry_idx >= 0; entry_idx--) {
		fat_lfn_entry *lfn = (fat_lfn_entry *)(dir_ptr + ((num_entries - 1 - entry_idx) * 32));
		int name_offset = entry_idx * 13;
		int j;

		memset(lfn, 0xFF, sizeof(fat_lfn_entry));

		lfn->sequence = (u8)(entry_idx + 1);
		if (entry_idx == num_entries - 1) {
			lfn->sequence |= LFN_LAST_ENTRY;
		}

		lfn->attr = FAT_ATTR_LONG_NAME;
		lfn->type = 0;
		lfn->checksum = checksum;
		lfn->first_cluster_lo = 0;

		/* Copy 5 characters to name1 */
		for (j = 0; j < 5; j++) {
			char_idx = name_offset + j;
			lfn->name1[j] = uni_name[char_idx];
		}

		/* Copy 6 characters to name2 */
		for (j = 0; j < 6; j++) {
			char_idx = name_offset + 5 + j;
			lfn->name2[j] = uni_name[char_idx];
		}

		/* Copy 2 characters to name3 */
		for (j = 0; j < 2; j++) {
			char_idx = name_offset + 11 + j;
			lfn->name3[j] = uni_name[char_idx];
		}
	}

	return num_entries;
}

/* Directory tree node for building arbitrary folder structures */
typedef struct _dir_node {
	char name[256];
	struct _dir_node *parent;
	struct _dir_node *first_child;
	struct _dir_node *next_sibling;
	u32 cluster;
	u32 entry_count;
	u32 clusters_allocated;
	u8 *data;
} dir_node;

/* Helper to count directory depth in file paths */
static int count_dir_levels(const file_entry_t* files, size_t count)
{
	size_t i;
	int max_depth = 0;

	for (i = 0; i < count; i++) {
		const wchar_t *p = files[i].path;
		int depth = 0;
		while (*p) {
			if (*p == L'\\' || *p == L'/') depth++;
			p++;
		}
		if (depth > max_depth) max_depth = depth;
	}
	return max_depth;
}

/* Helper to find or create a directory node */
static dir_node* find_or_create_dir(dir_node *parent, const char *name, u32 *total_dirs)
{
	dir_node *child;

	/* Search existing children */
	for (child = parent->first_child; child; child = child->next_sibling) {
		if (strcmp(child->name, name) == 0) {
			return child;
		}
	}

	/* Create new directory node */
	child = (dir_node*)calloc(1, sizeof(dir_node));
	if (!child) return NULL;

	strcpy_s(child->name, sizeof(child->name), name);
	child->parent = parent;

	/* Insert at beginning of children list */
	child->next_sibling = parent->first_child;
	parent->first_child = child;

	(*total_dirs)++;
	return child;
}

/* Count clusters needed for directory based on entry_count */
static u32 calculate_clusters_needed(u32 entry_count)
{
	u32 entries_per_cluster = FAT_SECTOR_SIZE / FAT_DIR_ENTRY_SIZE;
	u32 clusters = (entry_count + entries_per_cluster - 1) / entries_per_cluster;
	return (clusters > 0) ? clusters : 1; /* At least 1 cluster */
}

/* Free directory tree */
static void free_dir_tree(dir_node *node)
{
	dir_node *child, *next;

	if (!node) return;

	child = node->first_child;
	while (child) {
		next = child->next_sibling;
		free_dir_tree(child);
		child = next;
	}

	if (node->data) free(node->data);
	free(node);
}

/* Write FAT entry - handles both FAT12 and FAT16 */
static void write_fat_entry(u8 *fat_table, u32 cluster, u32 value, int is_fat16)
{
	if (is_fat16) {
		/* FAT16: 2 bytes per entry */
		u16 *fat16 = (u16*)fat_table;
		fat16[cluster] = (u16)value;
	} else {
		/* FAT12: 1.5 bytes per entry */
		u32 fat_offset = (cluster * 3) / 2;
		if (cluster & 1) {
			fat_table[fat_offset] = (fat_table[fat_offset] & 0x0F) | ((value & 0x0F) << 4);
			fat_table[fat_offset + 1] = (value >> 4) & 0xFF;
		} else {
			fat_table[fat_offset] = value & 0xFF;
			fat_table[fat_offset + 1] = (fat_table[fat_offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
		}
	}
}

/* Create a FAT12/16 image with files - supports arbitrary folder structures */
int create_fat_image(const char* label, const file_entry_t* files, size_t count, void **out_data, size_t *out_size)
{
	fat_boot_sector *boot;
	u8 *fat_img = NULL;
	u8 *fat_table;
	u8 *root_dir;
	u8 *data_area;
	u32 total_sectors;
	u32 fat_sectors;
	u32 root_dir_sectors;
	u32 data_sectors_start;
	u32 total_file_size = 0;
	u32 data_sectors_needed;
	u32 total_clusters_needed;
	u32 cluster;
	size_t i;
	int resl = ST_ERROR;
	dir_node *root_node = NULL;
	u32 total_dirs = 0;
	int max_depth;
	int is_fat16 = 0;
	u32 end_of_chain_marker;

	/* Count directory depth to estimate space needed */
	max_depth = count_dir_levels(files, count);

	/* Calculate total file size */
	for (i = 0; i < count; i++) {
		total_file_size += (u32)_align(files[i].size, FAT_SECTOR_SIZE);
	}

	/* Add space for subdirectories (2 clusters per directory for LFN support) */
	/* Estimate: max_depth levels * potential directories */
	total_file_size += (max_depth + 1) * 16 * FAT_SECTOR_SIZE;

	/* Calculate FAT parameters */
	/* Root directory: 224 entries = 14 sectors */
	root_dir_sectors = 14;
	total_clusters_needed = (total_file_size + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;
	data_sectors_needed = total_clusters_needed + 64;

	/* Decide between FAT12 and FAT16 based on cluster count */
	if (data_sectors_needed >= 4085) {
		/* Use FAT16 for larger volumes */
		is_fat16 = 1;
		end_of_chain_marker = 0xFFFF;
		/* FAT16 uses 2 bytes per cluster entry */
		fat_sectors = ((data_sectors_needed * 2) + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;
	} else {
		/* Use FAT12 for smaller volumes */
		is_fat16 = 0;
		end_of_chain_marker = 0xFFF;
		/* FAT12 uses 1.5 bytes per cluster entry */
		fat_sectors = ((data_sectors_needed * 3 / 2) + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;
		if (fat_sectors < 9) fat_sectors = 9;
	}

	total_sectors = 1 + (fat_sectors * 2) + root_dir_sectors + data_sectors_needed;

	/* Ensure it's within FAT16 limits (< 65525 clusters) */
	if (data_sectors_needed >= 65525) {
		return ST_ERROR;
	}

	do
	{
		if ((fat_img = calloc(1, total_sectors * FAT_SECTOR_SIZE)) == NULL) {
			resl = ST_NOMEM;
			break;
		}

		boot = (fat_boot_sector *)fat_img;
		boot->jmp_boot[0] = 0xEB;
		boot->jmp_boot[1] = 0x3C;
		boot->jmp_boot[2] = 0x90;
		mincpy(boot->oem_name, "MSWIN4.1", 8);
		boot->bytes_per_sector = FAT_SECTOR_SIZE;
		boot->sectors_per_cluster = 1;
		boot->reserved_sectors = 1;
		boot->num_fats = 2;
		boot->root_entries = 224;
		boot->total_sectors_16 = (u16)(total_sectors < 65536 ? total_sectors : 0);
		boot->media_type = 0xF8;
		boot->fat_size_16 = (u16)fat_sectors;
		boot->sectors_per_track = 63;
		boot->num_heads = 255;
		boot->hidden_sectors = 0;
		boot->total_sectors_32 = (total_sectors >= 65536) ? total_sectors : 0;
		boot->drive_number = 0x80;
		boot->reserved1 = 0;
		boot->boot_signature = 0x29;
		boot->volume_id = 0x12345678;
		mincpy(boot->volume_label, "           ", 11);
		strcpy_s(boot->volume_label, 11, label);
		mincpy(boot->fs_type, is_fat16 ? "FAT16   " : "FAT12   ", 8);
		boot->boot_sector_sig = 0xAA55;

		fat_table = fat_img + FAT_SECTOR_SIZE;

		/* Initialize first FAT entries based on type */
		if (is_fat16) {
			u16 *fat16 = (u16*)fat_table;
			fat16[0] = 0xFFF8; /* Media descriptor */
			fat16[1] = 0xFFFF; /* End of chain marker */
		} else {
			fat_table[0] = 0xF8;
			fat_table[1] = 0xFF;
			fat_table[2] = 0xFF;
		}

		mincpy(fat_img + FAT_SECTOR_SIZE + (fat_sectors * FAT_SECTOR_SIZE), fat_table, fat_sectors * FAT_SECTOR_SIZE);

		root_dir = fat_img + FAT_SECTOR_SIZE + (fat_sectors * 2 * FAT_SECTOR_SIZE);
		data_sectors_start = 1 + (fat_sectors * 2) + root_dir_sectors;
		data_area = fat_img + (data_sectors_start * FAT_SECTOR_SIZE);

		cluster = 2;

		/* Build directory tree from file paths */
		root_node = (dir_node*)calloc(1, sizeof(dir_node));
		if (!root_node) {
			resl = ST_NOMEM;
			break;
		}
		strcpy_s(root_node->name, sizeof(root_node->name), "<root>");

		/* Parse all file paths and build directory tree */
		for (i = 0; i < count; i++) {
			const wchar_t *path = files[i].path;
			const wchar_t *p;
			dir_node *current = root_node;
			char path_component[256];
			u32 j;

			/* Skip leading slashes */
			while (*path == L'\\' || *path == L'/') path++;

			/* Parse path components */
			while (*path) {
				/* Extract next path component */
				for (p = path, j = 0; *p && *p != L'\\' && *p != L'/' && j < 255; p++, j++) {
					path_component[j] = (char)*p;
				}
				path_component[j] = '\0';

				/* Skip to next component or end */
				if (*p == L'\\' || *p == L'/') {
					/* This is a directory - find or create it */
					current = find_or_create_dir(current, path_component, &total_dirs);
					if (!current) {
						resl = ST_NOMEM;
						break;
					}
					path = p + 1;
					while (*path == L'\\' || *path == L'/') path++; /* Skip multiple slashes */
				} else {
					/* This is the filename - we'll handle it in the next phase */
					path = p;
				}
			}

			if (resl == ST_NOMEM) break;
		}

		if (resl == ST_NOMEM) break;

		/* Allocate directory data buffers (16 clusters per directory = 8KB = 256 entries) */
		{
			dir_node *dir_queue[256];
			u32 queue_head = 0, queue_tail = 0;
			dir_node *current;

			/* BFS to allocate clusters and data for all directories */
			dir_queue[queue_tail++] = root_node;

			while (queue_head < queue_tail && queue_tail < 256) {
				current = dir_queue[queue_head++];

				/* Allocate data for this directory (16 clusters = 8192 bytes) */
				current->clusters_allocated = 16;
				current->data = (u8*)calloc(1, current->clusters_allocated * FAT_SECTOR_SIZE);
				if (!current->data) {
					resl = ST_NOMEM;
					break;
				}

				/* Assign cluster if not root */
				if (current != root_node) {
					u32 c;
					current->cluster = cluster;

					/* Allocate clusters for directory and chain them */
					for (c = 0; c < current->clusters_allocated; c++) {
						u32 next_cluster = (c == current->clusters_allocated - 1) ? end_of_chain_marker : (cluster + 1);
						write_fat_entry(fat_table, cluster, next_cluster, is_fat16);
						cluster++;
					}

					/* Write directory data to image */
					mincpy(data_area + ((current->cluster - 2) * FAT_SECTOR_SIZE), current->data, current->clusters_allocated * FAT_SECTOR_SIZE);

					/* Create "." and ".." entries */
					{
						fat_dir_entry *self = (fat_dir_entry *)(current->data);
						fat_dir_entry *parent = (fat_dir_entry *)(current->data + FAT_DIR_ENTRY_SIZE);

						memset(self->name, ' ', 11);
						self->name[0] = '.';
						self->attr = FAT_ATTR_DIRECTORY;
						self->first_cluster_lo = (u16)current->cluster;

						memset(parent->name, ' ', 11);
						parent->name[0] = '.';
						parent->name[1] = '.';
						parent->attr = FAT_ATTR_DIRECTORY;
						parent->first_cluster_lo = current->parent == root_node ? 0 : (u16)current->parent->cluster;

						current->entry_count = 2;
					}
				}

				/* Add all children to queue */
				{
					dir_node *child = current->first_child;
					while (child && queue_tail < 256) {
						dir_queue[queue_tail++] = child;
						child = child->next_sibling;
					}
				}
			}

			if (resl == ST_NOMEM) break;
		}

		/* Write directory entries for subdirectories */
		{
			dir_node *dir_queue[256];
			u32 queue_head = 0, queue_tail = 0;
			dir_node *current;

			dir_queue[queue_tail++] = root_node;

			while (queue_head < queue_tail && queue_tail < 256) {
				current = dir_queue[queue_head++];

				/* Write entries for each child directory */
				{
					dir_node *child = current->first_child;
					while (child) {
						u8 *target_dir = (current == root_node) ? root_dir : current->data;
						u32 entry_offset = (current == root_node) ? 0 : current->entry_count;
						u8 short_name[11];
						u8 chksum;
						int lfn_count = 0;
						fat_dir_entry *entry;

						/* Generate short name and write LFN entries */
						generate_short_name(child->name, short_name, 1);
						chksum = lfn_checksum(short_name);
						lfn_count = write_lfn_entries(target_dir + (entry_offset * FAT_DIR_ENTRY_SIZE), child->name, chksum);

						/* Write directory entry */
						entry = (fat_dir_entry *)(target_dir + ((entry_offset + lfn_count) * FAT_DIR_ENTRY_SIZE));
						memcpy(entry->name, short_name, 11);
						entry->attr = FAT_ATTR_DIRECTORY;
						entry->first_cluster_lo = (u16)child->cluster;

						current->entry_count += (lfn_count + 1);

						/* Update data area for subdirectories */
						if (current != root_node) {
							mincpy(data_area + ((current->cluster - 2) * FAT_SECTOR_SIZE), current->data, current->clusters_allocated * FAT_SECTOR_SIZE);
						}

						child = child->next_sibling;
					}
				}

				/* Add all children to queue */
				{
					dir_node *child = current->first_child;
					while (child && queue_tail < 256) {
						dir_queue[queue_tail++] = child;
						child = child->next_sibling;
					}
				}
			}
		}

		/* Add files to their respective directories */
		for (i = 0; i < count; i++) {
			const wchar_t *path = files[i].path;
			const wchar_t *p;
			const wchar_t *filename_start = path;
			dir_node *target_dir = root_node;
			char path_component[256];
			char filename[256];
			u32 j;

			/* Skip leading slashes */
			while (*path == L'\\' || *path == L'/') path++;

			/* Navigate to target directory */
			while (*path) {
				/* Extract next path component */
				for (p = path, j = 0; *p && *p != L'\\' && *p != L'/' && j < 255; p++, j++) {
					path_component[j] = (char)*p;
				}
				path_component[j] = '\0';

				if (*p == L'\\' || *p == L'/') {
					/* This is a directory - find it */
					dir_node *child = target_dir->first_child;
					while (child) {
						if (strcmp(child->name, path_component) == 0) {
							target_dir = child;
							break;
						}
						child = child->next_sibling;
					}
					path = p + 1;
					while (*path == L'\\' || *path == L'/') path++;
					filename_start = path;
				} else {
					/* This is the filename */
					filename_start = path;
					path = p;
				}
			}

			/* Extract filename */
			for (p = filename_start, j = 0; *p && j < 255; p++, j++) {
				filename[j] = (char)*p;
			}
			filename[j] = '\0';

			if (filename[0] != '\0') {
				u8 *target_data = (target_dir == root_node) ? root_dir : target_dir->data;
				u32 entry_offset = (target_dir == root_node) ? 0 : target_dir->entry_count;
				u8 short_name[11];
				u8 chksum;
				int lfn_count = 0;
				fat_dir_entry *entry;

				/* Find next available entry in root directory if needed */
				if (target_dir == root_node) {
					while (entry_offset < 224) {
						fat_dir_entry *check = (fat_dir_entry *)(root_dir + (entry_offset * FAT_DIR_ENTRY_SIZE));
						if (check->name[0] == 0 || check->name[0] == 0xE5) break;
						entry_offset++;
					}
				}

				/* Generate short name and write LFN entries */
				generate_short_name(filename, short_name, 1);
				chksum = lfn_checksum(short_name);
				lfn_count = write_lfn_entries(target_data + (entry_offset * FAT_DIR_ENTRY_SIZE), filename, chksum);

				/* Write file entry */
				entry = (fat_dir_entry *)(target_data + ((entry_offset + lfn_count) * FAT_DIR_ENTRY_SIZE));
				memcpy(entry->name, short_name, 11);
				entry->attr = FAT_ATTR_ARCHIVE;
				entry->first_cluster_lo = (u16)cluster;
				entry->file_size = (u32)files[i].size;

				if (target_dir != root_node) {
					target_dir->entry_count += (lfn_count + 1);
				}

				/* Write file data */
				if (files[i].data && files[i].size > 0) {
					u32 file_sectors = (u32)((files[i].size + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE);
					mincpy(data_area + ((cluster - 2) * FAT_SECTOR_SIZE), files[i].data, files[i].size);

					for (j = 0; j < file_sectors; j++) {
						u32 next_cluster = (j == file_sectors - 1) ? end_of_chain_marker : (cluster + 1);
						write_fat_entry(fat_table, cluster, next_cluster, is_fat16);
						cluster++;
					}
				}

				/* Update directory data in image for subdirectories */
				if (target_dir != root_node) {
					mincpy(data_area + ((target_dir->cluster - 2) * FAT_SECTOR_SIZE), target_dir->data, target_dir->clusters_allocated * FAT_SECTOR_SIZE);
				}
			}
		}

		/* Copy FAT table to second FAT */
		mincpy(fat_img + FAT_SECTOR_SIZE + (fat_sectors * FAT_SECTOR_SIZE), fat_table, fat_sectors * FAT_SECTOR_SIZE);

		*out_data = fat_img;
		*out_size = total_sectors * FAT_SECTOR_SIZE;
		resl = ST_OK;

	} while (0);

	/* Cleanup */
	if (root_node) {
		free_dir_tree(root_node);
	}

	if (resl != ST_OK && fat_img) {
		free(fat_img);
	}

	return resl;
}

/* Read FAT entry - handles both FAT12 and FAT16 */
static u32 read_fat_entry(const u8 *fat_table, u32 cluster, int is_fat16)
{
	if (is_fat16) {
		/* FAT16: 2 bytes per entry */
		const u16 *fat16 = (const u16*)fat_table;
		return fat16[cluster];
	} else {
		/* FAT12: 1.5 bytes per entry */
		u32 fat_offset = (cluster * 3) / 2;
		if (cluster & 1) {
			return ((fat_table[fat_offset] >> 4) | (fat_table[fat_offset + 1] << 4)) & 0xFFF;
		} else {
			return (fat_table[fat_offset] | ((fat_table[fat_offset + 1] & 0x0F) << 8)) & 0xFFF;
		}
	}
}

/* Extract long filename from LFN entries */
static int extract_lfn(const u8 *dir_data, u32 entry_index, char *lfn_out, size_t lfn_out_size)
{
	u32 i = entry_index;
	char lfn_parts[20][14]; /* Max 20 LFN entries, 13 chars each + null terminator */
	int num_parts = 0;
	int lfn_found = 0;
	int found_last = 0;
	u32 j;

	memset(lfn_parts, 0, sizeof(lfn_parts));

	/* Look backwards for LFN entries */
	if (i > 0) {
		i--;
		while (i != (u32)-1) {
			fat_lfn_entry *lfn = (fat_lfn_entry *)(dir_data + (i * FAT_DIR_ENTRY_SIZE));
			int seq_num;
			int part_idx = 0;

			/* Check if this is an LFN entry */
			if ((lfn->attr & FAT_ATTR_LONG_NAME) != FAT_ATTR_LONG_NAME) {
				break;
			}

			lfn_found = 1;

			/* Get sequence number (remove LFN_LAST_ENTRY flag) */
			seq_num = (lfn->sequence & ~LFN_LAST_ENTRY) - 1;

			if (seq_num >= 0 && seq_num < 20) {
				/* Extract characters from LFN entry */
				for (j = 0; j < 5 && part_idx < 13; j++) {
					if (lfn->name1[j] == 0 || lfn->name1[j] == 0xFFFF) goto done_extracting;
					lfn_parts[seq_num][part_idx++] = (char)(lfn->name1[j] & 0xFF);
				}
				for (j = 0; j < 6 && part_idx < 13; j++) {
					if (lfn->name2[j] == 0 || lfn->name2[j] == 0xFFFF) goto done_extracting;
					lfn_parts[seq_num][part_idx++] = (char)(lfn->name2[j] & 0xFF);
				}
				for (j = 0; j < 2 && part_idx < 13; j++) {
					if (lfn->name3[j] == 0 || lfn->name3[j] == 0xFFFF) goto done_extracting;
					lfn_parts[seq_num][part_idx++] = (char)(lfn->name3[j] & 0xFF);
				}
done_extracting:
				lfn_parts[seq_num][part_idx] = '\0';
				if (seq_num >= num_parts) {
					num_parts = seq_num + 1;
				}
			}

			/* Mark that we found the last entry, but continue to get all parts */
			if (lfn->sequence & LFN_LAST_ENTRY) {
				found_last = 1;
			}

			/* If we found the last entry and this is sequence 1, we have all parts */
			if (found_last && seq_num == 0) {
				break;
			}

			if (i == 0) break;
			i--;
		}
	}

	if (lfn_found && num_parts > 0) {
		int out_pos = 0;
		/* Concatenate parts in correct order (0, 1, 2, ...) */
		for (i = 0; i < (u32)num_parts && out_pos < (int)lfn_out_size - 1; i++) {
			for (j = 0; lfn_parts[i][j] && out_pos < (int)lfn_out_size - 1; j++) {
				lfn_out[out_pos++] = lfn_parts[i][j];
			}
		}
		lfn_out[out_pos] = '\0';
		return 1;
	}

	return 0;
}

/* Get FAT volume label */
int get_fat_label(char* in_data, size_t in_size, char** label, size_t* label_len)
{
	fat_boot_sector *boot;
	u8 *root_dir;
	u32 root_dir_sectors;
	u32 fat_sectors;
	u32 i;

	if (!in_data || in_size < FAT_SECTOR_SIZE || !label || !label_len) {
		return ST_ERROR;
	}

	boot = (fat_boot_sector *)in_data;

	/* Verify boot signature */
	if (boot->boot_sector_sig != 0xAA55) {
		return ST_ERROR;
	}

	/* Get FAT parameters */
	fat_sectors = boot->fat_size_16;
	root_dir_sectors = ((boot->root_entries * FAT_DIR_ENTRY_SIZE) + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;

	/* Calculate root directory location */
	root_dir = (u8*)(in_data + FAT_SECTOR_SIZE + (fat_sectors * boot->num_fats * FAT_SECTOR_SIZE));

	/* Search for volume label entry in root directory */
	for (i = 0; i < boot->root_entries; i++) {
		fat_dir_entry *entry = (fat_dir_entry *)(root_dir + (i * FAT_DIR_ENTRY_SIZE));

		/* Check if this is a volume label entry */
		if ((entry->attr & FAT_ATTR_VOLUME_ID) && !(entry->attr & FAT_ATTR_DIRECTORY)) {
			/* Found volume label */
			*label = (char*)entry->name;
			*label_len = 11;
			return ST_OK;
		}

		/* End of directory entries */
		if (entry->name[0] == 0) {
			break;
		}
	}

	/* If no volume label found in root directory, use the one from boot sector */
	*label = (char*)boot->volume_label;
	*label_len = 11;
	return ST_OK;
}

/* Find file by path and return pointer to data and size */
int get_fat_file(char* in_data, size_t in_size, const wchar_t* filePath, char** file, size_t* file_size)
{
	fat_boot_sector *boot;
	u8 *fat_table;
	u8 *root_dir;
	u8 *data_area;
	u32 root_dir_sectors;
	u32 fat_sectors;
	u32 data_sectors_start;
	wchar_t path_copy[512];
	wchar_t *token;
	wchar_t *next_token = NULL;
	int is_fat16;
	u32 current_cluster = 0;
	u32 dir_entries;
	u8 *current_dir;
	int is_root = 1;
	const wchar_t *filename = NULL;

	if (!in_data || in_size < FAT_SECTOR_SIZE || !filePath || !file || !file_size) {
		return ST_ERROR;
	}

	boot = (fat_boot_sector *)in_data;

	/* Verify boot signature */
	if (boot->boot_sector_sig != 0xAA55) {
		return ST_ERROR;
	}

	/* Determine FAT type */
	is_fat16 = (memcmp(boot->fs_type, "FAT16   ", 8) == 0);

	/* Get FAT parameters */
	fat_sectors = boot->fat_size_16;
	root_dir_sectors = ((boot->root_entries * FAT_DIR_ENTRY_SIZE) + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;

	/* Calculate locations */
	fat_table = (u8*)(in_data + FAT_SECTOR_SIZE);
	root_dir = (u8*)(in_data + FAT_SECTOR_SIZE + (fat_sectors * boot->num_fats * FAT_SECTOR_SIZE));
	data_sectors_start = 1 + (fat_sectors * boot->num_fats) + root_dir_sectors;
	data_area = (u8*)(in_data + (data_sectors_start * FAT_SECTOR_SIZE));

	/* Start at root directory */
	current_dir = root_dir;
	dir_entries = boot->root_entries;

	/* Parse path */
	wcscpy_s(path_copy, sizeof(path_copy) / sizeof(wchar_t), filePath);

	/* Skip leading slashes */
	token = path_copy;
	while (*token == L'\\' || *token == L'/') token++;

	/* Tokenize path */
	token = wcstok_s(token, L"\\/", &next_token);

	while (token != NULL) {
		char next_component[256] = {0};
		u32 i;
		int found = 0;

		/* Check if there's another component after this one */
		wchar_t *peek = next_token;
		while (peek && (*peek == L'\\' || *peek == L'/')) peek++;

		if (peek && *peek) {
			/* This is a directory component - convert to char */
			size_t converted;
			wcstombs_s(&converted, next_component, sizeof(next_component), token, _TRUNCATE);
		} else {
			/* This is the filename */
			filename = token;
			break;
		}

		/* Search for directory entry */
		for (i = 0; i < dir_entries; i++) {
			fat_dir_entry *entry = (fat_dir_entry *)(current_dir + (i * FAT_DIR_ENTRY_SIZE));
			char entry_name[256] = {0};
			char lfn_name[256] = {0};
			u32 j, k = 0;
			int has_lfn = 0;

			/* End of directory */
			if (entry->name[0] == 0) {
				break;
			}

			/* Skip deleted entries */
			if (entry->name[0] == 0xE5) {
				continue;
			}

			/* Skip LFN entries and volume labels */
			if ((entry->attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME ||
			    (entry->attr & FAT_ATTR_VOLUME_ID)) {
				continue;
			}

			/* Skip non-directories */
			if (!(entry->attr & FAT_ATTR_DIRECTORY)) {
				continue;
			}

			/* Try to extract LFN */
			has_lfn = extract_lfn(current_dir, i, lfn_name, sizeof(lfn_name));

			/* Convert 8.3 name to regular name */
			for (j = 0; j < 8 && entry->name[j] != ' '; j++) {
				entry_name[k++] = entry->name[j];
			}
			entry_name[k] = '\0';

			/* Compare directory name (case insensitive) - check both LFN and 8.3 name */
			if ((has_lfn && _stricmp(lfn_name, next_component) == 0) ||
			    _stricmp(entry_name, next_component) == 0) {
				/* Found the directory */
				current_cluster = entry->first_cluster_lo | ((u32)entry->first_cluster_hi << 16);

				/* Check if cluster is valid */
				if (current_cluster < 2) {
					return ST_ERROR;
				}

				/* Follow FAT chain to count total clusters */
				u32 temp_cluster = current_cluster;
				u32 cluster_count = 0;
				u32 end_marker = is_fat16 ? 0xFFF8 : 0xFF8;

				while (temp_cluster >= 2 && temp_cluster < end_marker) {
					cluster_count++;
					temp_cluster = read_fat_entry(fat_table, temp_cluster, is_fat16);
				}

				/* Switch to the subdirectory */
				current_dir = data_area + ((current_cluster - 2) * boot->sectors_per_cluster * FAT_SECTOR_SIZE);
				dir_entries = (cluster_count * boot->sectors_per_cluster * FAT_SECTOR_SIZE) / FAT_DIR_ENTRY_SIZE;
				is_root = 0;
				found = 1;
				break;
			}
		}

		if (!found) {
			/* Directory not found */
			return ST_ERROR;
		}

		/* Move to next path component */
		token = wcstok_s(NULL, L"\\/", &next_token);
	}

	/* Now search for the file in the current directory */
	if (filename) {
		u32 i;
		char filename_char[256] = {0};
		size_t converted;

		/* Convert wide char filename to char */
		wcstombs_s(&converted, filename_char, sizeof(filename_char), filename, _TRUNCATE);

		for (i = 0; i < dir_entries; i++) {
			fat_dir_entry *entry = (fat_dir_entry *)(current_dir + (i * FAT_DIR_ENTRY_SIZE));
			char entry_name[256] = {0};
			char lfn_name[256] = {0};
			u32 j, k = 0;
			int has_lfn = 0;

			/* End of directory */
			if (entry->name[0] == 0) {
				break;
			}

			/* Skip deleted entries */
			if (entry->name[0] == 0xE5) {
				continue;
			}

			/* Skip LFN entries and volume labels */
			if ((entry->attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME ||
			    (entry->attr & FAT_ATTR_VOLUME_ID)) {
				continue;
			}

			/* Skip directories */
			if (entry->attr & FAT_ATTR_DIRECTORY) {
				continue;
			}

			/* Try to extract LFN */
			has_lfn = extract_lfn(current_dir, i, lfn_name, sizeof(lfn_name));

			/* Convert 8.3 name to regular name */
			for (j = 0; j < 8 && entry->name[j] != ' '; j++) {
				entry_name[k++] = entry->name[j];
			}

			/* Add extension if present */
			if (entry->name[8] != ' ') {
				entry_name[k++] = '.';
				for (j = 8; j < 11 && entry->name[j] != ' '; j++) {
					entry_name[k++] = entry->name[j];
				}
			}
			entry_name[k] = '\0';

			/* Compare filename (case insensitive) - check both LFN and 8.3 name */
			if ((has_lfn && _stricmp(lfn_name, filename_char) == 0) ||
			    _stricmp(entry_name, filename_char) == 0) {
				/* Found the file */
				u32 file_cluster = entry->first_cluster_lo | ((u32)entry->first_cluster_hi << 16);
				u32 cluster_count = 0;
				u32 temp_cluster = file_cluster;
				u32 end_marker = is_fat16 ? 0xFFF8 : 0xFF8;

				/* Check if file is empty */
				if (entry->file_size == 0) {
					*file = NULL;
					*file_size = 0;
					return ST_OK;
				}

				/* Check if file is fragmented by counting clusters */
				while (temp_cluster >= 2 && temp_cluster < end_marker) {
					cluster_count++;
					u32 next_cluster = read_fat_entry(fat_table, temp_cluster, is_fat16);

					/* Check if next cluster is contiguous */
					if (next_cluster < end_marker && next_cluster != temp_cluster + 1) {
						/* File is fragmented */
						return ST_ERROR;
					}

					temp_cluster = next_cluster;
				}

				/* Calculate file location */
				*file = (char*)(data_area + ((file_cluster - 2) * boot->sectors_per_cluster * FAT_SECTOR_SIZE));
				*file_size = entry->file_size;
				return ST_OK;
			}
		}
	}

	/* File not found */
	return ST_ERROR;
}


/* Check if a FAT entry is end-of-chain */
static int is_end_of_chain(u32 value, int is_fat16)
{
	if (is_fat16) {
		return value >= 0xFFF8;
	} else {
		return value >= 0xFF8;
	}
}

/* Parse LFN entries and construct long filename */
static int parse_lfn_entries(fat_dir_entry *entries, int max_entries, wchar_t *long_name, size_t long_name_size, u8 expected_checksum)
{
	int i, j;
	wchar_t lfn_buffer[260];
	int lfn_chars = 0;
	int found_last = 0;

	memset(lfn_buffer, 0, sizeof(lfn_buffer));

	/* Collect LFN entries */
	for (i = 0; i < max_entries; i++) {
		fat_lfn_entry *lfn = (fat_lfn_entry *)&entries[i];

		/* Stop if not an LFN entry */
		if (lfn->attr != FAT_ATTR_LONG_NAME) {
			break;
		}

		/* Verify checksum matches */
		if (lfn->checksum != expected_checksum) {
			return 0; /* Checksum mismatch */
		}

		/* Get sequence number without flags */
		int seq = lfn->sequence & 0x3F;
		if (seq < 1 || seq > 20) {
			return 0; /* Invalid sequence number */
		}

		int base_idx = (seq - 1) * 13;

		/* Extract 13 characters from this LFN entry */
		for (j = 0; j < 5 && base_idx + j < 260; j++) {
			wchar_t ch = lfn->name1[j];
			if (ch == 0x0000 || ch == 0xFFFF) {
				goto done_extracting_entry;
			}
			lfn_buffer[base_idx + j] = ch;
		}
		for (j = 0; j < 6 && base_idx + 5 + j < 260; j++) {
			wchar_t ch = lfn->name2[j];
			if (ch == 0x0000 || ch == 0xFFFF) {
				goto done_extracting_entry;
			}
			lfn_buffer[base_idx + 5 + j] = ch;
		}
		for (j = 0; j < 2 && base_idx + 11 + j < 260; j++) {
			wchar_t ch = lfn->name3[j];
			if (ch == 0x0000 || ch == 0xFFFF) {
				goto done_extracting_entry;
			}
			lfn_buffer[base_idx + 11 + j] = ch;
		}

done_extracting_entry:
		/* Check if this is the last entry */
		if (lfn->sequence & LFN_LAST_ENTRY) {
			found_last = 1;
		}
	}

	/* Must have found at least one entry and the last entry marker */
	if (i == 0 || !found_last) {
		return 0;
	}

	/* Find actual length (stop at null or 0xFFFF) */
	for (lfn_chars = 0; lfn_chars < 260 && lfn_buffer[lfn_chars] != 0 && lfn_buffer[lfn_chars] != 0xFFFF; lfn_chars++);

	/* Must have at least one character */
	if (lfn_chars == 0) {
		return 0;
	}

	/* Copy to output buffer if it fits (including null terminator) */
	if (lfn_chars + 1 <= (int)long_name_size) {
		memcpy(long_name, lfn_buffer, lfn_chars * sizeof(wchar_t));
		long_name[lfn_chars] = 0;
		return 1; /* Success */
	}

	/* Filename too long */
	return 0;
}

/* Convert 8.3 FAT name to regular filename */
static void fat_name_to_string(const u8 *fat_name, wchar_t *output, size_t output_size)
{
	int i, pos = 0;

	/* Copy name part (up to 8 chars) */
	for (i = 0; i < 8 && pos < output_size - 1; i++) {
		if (fat_name[i] == ' ') break;
		output[pos++] = (wchar_t)fat_name[i];
	}

	/* Check if there's an extension */
	if (fat_name[8] != ' ') {
		if (pos < output_size - 1) {
			output[pos++] = L'.';
		}
		for (i = 8; i < 11 && pos < output_size - 1; i++) {
			if (fat_name[i] == ' ') break;
			output[pos++] = (wchar_t)fat_name[i];
		}
	}

	output[pos] = 0;
}

/* Recursive directory traversal */
static int extract_directory(
	u8 *fat_img,
	u8 *fat_table,
	u8 *data_area,
	fat_dir_entry *dir_entries,
	u32 num_entries,
	const wchar_t *current_path,
	file_entry_t **file_list,
	size_t *file_count,
	size_t *file_capacity,
	int is_fat16,
	u32 sectors_per_cluster,
	u32 data_start_cluster
)
{
	u32 i;

	for (i = 0; i < num_entries; i++) {
		fat_dir_entry *entry = &dir_entries[i];
		wchar_t long_name[260];
		wchar_t short_name[13];
		wchar_t *entry_name;
		wchar_t full_path[1024];
		u32 first_cluster;

		/* End of directory */
		if (entry->name[0] == 0x00) {
			break;
		}

		/* Skip deleted entries */
		if (entry->name[0] == 0xE5) {
			continue;
		}

		/* Skip volume labels */
		if (entry->attr & FAT_ATTR_VOLUME_ID) {
			continue;
		}

		/* Skip LFN entries (will be processed with their associated 8.3 entry) */
		if (entry->attr == FAT_ATTR_LONG_NAME) {
			continue;
		}

		/* Skip "." and ".." */
		if (entry->name[0] == '.' && (entry->name[1] == ' ' || entry->name[1] == '.')) {
			continue;
		}

		/* Try to parse LFN entries before this entry */
		long_name[0] = 0;

		/* Look backwards for LFN entries */
		if (i > 0) {
			u32 lfn_start = i;
			u32 lfn_count_found = 0;
			u8 expected_checksum = lfn_checksum(entry->name);

			/* Find the start of the LFN sequence by looking backwards */
			while (lfn_start > 0) {
				fat_dir_entry *prev_entry = &dir_entries[lfn_start - 1];
				if (prev_entry->attr != FAT_ATTR_LONG_NAME) {
					break;
				}
				lfn_start--;
				lfn_count_found++;
			}

			/* If we found LFN entries, parse them */
			if (lfn_count_found > 0) {
				int success = parse_lfn_entries(&dir_entries[lfn_start], (int)lfn_count_found, long_name, 260, expected_checksum);
				/* If parsing failed, clear long_name */
				if (!success) {
					long_name[0] = 0;
				}
			}
		}

		/* Use long name if available, otherwise convert short name */
		if (long_name[0] != 0) {
			entry_name = long_name;
		} else {
			fat_name_to_string(entry->name, short_name, 13);
			entry_name = short_name;
		}

		/* Build full path */
		if (current_path[0] != 0) {
			_snwprintf(full_path, 1024, L"%s\\%s", current_path, entry_name);
		} else {
			_snwprintf(full_path, 1024, L"%s", entry_name);
		}
		full_path[1023] = 0;

		first_cluster = entry->first_cluster_lo | ((u32)entry->first_cluster_hi << 16);

		if (entry->attr & FAT_ATTR_DIRECTORY) {
			/* It's a directory - recurse into it */
			if (first_cluster >= 2) {
				u32 cluster = first_cluster;
				u32 cluster_count = 0;
				u32 temp_cluster;
				u8 *dir_buffer = NULL;
				u32 total_entries;

				/* First, count how many clusters this directory has */
				temp_cluster = first_cluster;
				while (!is_end_of_chain(temp_cluster, is_fat16)) {
					if (temp_cluster < 2 || temp_cluster >= 0xFFF0) break;
					cluster_count++;
					temp_cluster = read_fat_entry(fat_table, temp_cluster, is_fat16);
				}

				if (cluster_count > 0) {
					/* Allocate buffer for all directory entries */
					u32 dir_size = cluster_count * sectors_per_cluster * FAT_SECTOR_SIZE;
					dir_buffer = (u8*)malloc(dir_size);
					if (dir_buffer) {
						u32 offset = 0;

						/* Read all clusters into the buffer */
						cluster = first_cluster;
						while (!is_end_of_chain(cluster, is_fat16) && offset < dir_size) {
							if (cluster < 2 || cluster >= 0xFFF0) break;

							u8 *cluster_data = data_area + ((cluster - data_start_cluster) * sectors_per_cluster * FAT_SECTOR_SIZE);
							u32 cluster_size = sectors_per_cluster * FAT_SECTOR_SIZE;
							memcpy(dir_buffer + offset, cluster_data, cluster_size);
							offset += cluster_size;

							cluster = read_fat_entry(fat_table, cluster, is_fat16);
						}

						/* Process all directory entries at once */
						total_entries = dir_size / FAT_DIR_ENTRY_SIZE;
						extract_directory(
							fat_img, fat_table, data_area,
							(fat_dir_entry *)dir_buffer,
							total_entries,
							full_path,
							file_list, file_count, file_capacity,
							is_fat16, sectors_per_cluster, data_start_cluster
						);

						free(dir_buffer);
					}
				}
			}
		} else {
			/* It's a file - extract it */
			if (*file_count >= *file_capacity) {
				size_t new_capacity = *file_capacity * 2;
				file_entry_t *new_list = (file_entry_t *)realloc(*file_list, new_capacity * sizeof(file_entry_t));
				if (!new_list) return ST_NOMEM;
				*file_list = new_list;
				*file_capacity = new_capacity;
			}

			file_entry_t *fe = &(*file_list)[*file_count];
			wcscpy(fe->path, full_path);
			fe->size = entry->file_size;

			if (entry->file_size > 0 && first_cluster >= 2) {
				/* Allocate buffer for file data */
				fe->data = malloc(entry->file_size);
				if (!fe->data) {
					free((wchar_t*)fe->path);
					return ST_NOMEM;
				}

				/* Read file clusters */
				u32 cluster = first_cluster;
				u32 bytes_read = 0;

				while (!is_end_of_chain(cluster, is_fat16) && bytes_read < entry->file_size) {
					if (cluster < 2 || cluster >= 0xFFF0) break;

					u8 *cluster_data = data_area + ((cluster - data_start_cluster) * sectors_per_cluster * FAT_SECTOR_SIZE);
					u32 bytes_to_copy = min(sectors_per_cluster * FAT_SECTOR_SIZE, entry->file_size - bytes_read);

					memcpy((u8*)fe->data + bytes_read, cluster_data, bytes_to_copy);
					bytes_read += bytes_to_copy;

					cluster = read_fat_entry(fat_table, cluster, is_fat16);
				}
			} else {
				fe->data = NULL;
			}

			(*file_count)++;
		}
	}

	return ST_OK;
}

int extract_fat_image(void *in_data, size_t in_size, file_entry_t** files, size_t* count)
{
	fat_boot_sector *boot;
	u8 *fat_img = (u8*)in_data;
	u8 *fat_table;
	u8 *root_dir;
	u8 *data_area;
	u32 fat_sectors;
	u32 root_dir_sectors;
	u32 data_sectors_start;
	u32 root_entries;
	int is_fat16 = 0;
	size_t file_count = 0;
	size_t file_capacity = 16;
	int resl = ST_ERROR;
	size_t i;

	if (!in_data || in_size < FAT_SECTOR_SIZE) {
		return ST_ERROR;
	}

	boot = (fat_boot_sector *)fat_img;

	/* Verify boot signature */
	if (boot->boot_sector_sig != 0xAA55) {
		return ST_ERROR;
	}

	/* Verify it's FAT12/16 */
	if (boot->bytes_per_sector != FAT_SECTOR_SIZE) {
		return ST_ERROR;
	}

	/* Determine FAT type */
	if (memcmp(boot->fs_type, "FAT16", 5) == 0) {
		is_fat16 = 1;
	} else if (memcmp(boot->fs_type, "FAT12", 5) == 0) {
		is_fat16 = 0;
	} else {
		/* Try to determine from cluster count */
		fat_sectors = boot->fat_size_16;
		u32 total_sectors = boot->total_sectors_16 ? boot->total_sectors_16 : boot->total_sectors_32;
		root_dir_sectors = ((boot->root_entries * 32) + (boot->bytes_per_sector - 1)) / boot->bytes_per_sector;
		u32 data_sectors = total_sectors - (boot->reserved_sectors + (boot->num_fats * fat_sectors) + root_dir_sectors);
		u32 cluster_count = data_sectors / boot->sectors_per_cluster;

		is_fat16 = (cluster_count >= 4085);
	}

	fat_sectors = boot->fat_size_16;
	root_entries = boot->root_entries;
	root_dir_sectors = ((root_entries * 32) + (FAT_SECTOR_SIZE - 1)) / FAT_SECTOR_SIZE;

	fat_table = fat_img + (boot->reserved_sectors * FAT_SECTOR_SIZE);
	root_dir = fat_table + (boot->num_fats * fat_sectors * FAT_SECTOR_SIZE);
	data_sectors_start = boot->reserved_sectors + (boot->num_fats * fat_sectors) + root_dir_sectors;
	data_area = fat_img + (data_sectors_start * FAT_SECTOR_SIZE);

	/* Allocate initial file list */
	*files = (file_entry_t *)calloc(file_capacity, sizeof(file_entry_t));
	if (!*files) {
		return ST_NOMEM;
	}

	/* Extract files starting from root directory */
	resl = extract_directory(
		fat_img, fat_table, data_area,
		(fat_dir_entry *)root_dir,
		root_entries,
		L"",
		files, &file_count, &file_capacity,
		is_fat16,
		boot->sectors_per_cluster,
		2
	);

	if (resl != ST_OK) {
		/* Cleanup on error */
		for (i = 0; i < file_count; i++) {
			free((wchar_t*)(*files)[i].path);
			if ((*files)[i].data) free((*files)[i].data);
		}
		free(*files);
		*files = NULL;
	}

	return resl;
}