#ifndef _MAKE_ISO_
#define _MAKE_ISO_

typedef struct
{
	wchar_t path[MAX_PATH];
	size_t size;
	void* data;
} file_entry_t;


int create_iso_image(const char* label, const file_entry_t* files, size_t count, const file_entry_t* boot_file, void **out_data, size_t *out_size);
int extract_iso_image(void *in_data, size_t in_size, file_entry_t** files, size_t* count);
int get_iso_label(char* in_data, size_t in_size, char** label, size_t* label_len);
int get_iso_file(char* in_data, size_t in_size, const wchar_t* filePath, char** file, size_t* file_size);

int create_fat_image(const char* label, const file_entry_t* files, size_t count, void **out_data, size_t *out_size);
int extract_fat_image(void *in_data, size_t in_size, file_entry_t** files, size_t* count);
int get_fat_label(char* in_data, size_t in_size, char** label, size_t* label_len);
int get_fat_file(char* in_data, size_t in_size, const wchar_t* filePath, char** file, size_t* file_size);

#endif