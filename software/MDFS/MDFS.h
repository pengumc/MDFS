#ifndef _MDFS_H_
#define _MDFS_H_

#include <stdint.h>
/** @brief Maximum number of chars in a name, \0 included */
#define MDFS_MAX_FILENAME (116)

#define MDFS_BLOCKSIZE (65536)
#define MDFS_MAX_FILESIZE (1073741824) // = 1 GB
#define MDFS_ERROR_LEN (80)
#define MDFS_STATE_CLOSED (0)
#define MDFS_STATE_OPEN (1)
#define MDFS_EOF EOF
#define MDFS_EXTRA_CRC_SIZE (8) // Bytes to append for CRC to file list

typedef struct _mdfs_iobuf
{
  int index; ///< Index in file list at time of opening
  uint32_t offset; ///< Read position
  void* base; ///< Absolute start address
  int32_t size;
	uint32_t crc; ///< Copied at time of opening
  char filename[MDFS_MAX_FILENAME];
} mdfs_FILE;

// Structure of a entry in the file list
typedef struct MDFSFile {
	int32_t size;
	uint32_t byte_offset; ///< From start of FS (yes I don't expect > 4 GB)
	uint32_t crc;
	char filename[MDFS_MAX_FILENAME];
} mdfs_file_t;
#define MDFS_MAX_FILECOUNT (MDFS_BLOCKSIZE/sizeof(struct MDFSFile)-1) // = 511

typedef struct MDFS {
	const void* target;
	mdfs_file_t* file_list; ///< List is ordered by byte_offset
	uint32_t file_count; ///< Number of entries in file_list
	char error[MDFS_ERROR_LEN]; ///< Buffer for error msg. Always a valid string.
} mdfs_t;


mdfs_t* mdfs_init_simple(const void* target);
void mdfs_deinit(mdfs_t* mdfs);
int mdfs_get_filename(mdfs_t* mdfs, int index, char* buffer);
int32_t mdfs_get_filesize(mdfs_t* mdfs, int index);
uint32_t mdfs_get_file_offset(mdfs_t* mdfs, int index);
uint32_t mdfs_get_file_crc(mdfs_t* mdfs, int index);
uint32_t mdfs_add_file(mdfs_t* mdfs, const char* filename, int32_t size);
int mdfs_remove_file(mdfs_t* mdfs, const char* filename);

// IO functions
mdfs_FILE* mdfs_fopen(mdfs_t* mdfs, const char* filename, const char* mode);
mdfs_FILE* mdfs_freopen(mdfs_t* mdfs, const char* filename, const char* mode, mdfs_FILE* f);
int mdfs_fclose(mdfs_FILE* f);
size_t mdfs_fread(void* ptr, size_t size, size_t count, mdfs_FILE* f);
int mdfs_feof(mdfs_FILE* f);
int mdfs_fgetc(mdfs_FILE* f);
#define mdfs_ferror(f) (0)
#define mdfs_getc(f) mdfs_fgetc(f)
#define mdfs_passthrough_stdin(mdfs) mdfs_fopen((mdfs), "stdin", "r")
inline size_t mdfs_get_file_list_size(mdfs_t* mdfs) __attribute__((always_inline));
inline size_t mdfs_get_file_list_size(mdfs_t* mdfs) { 
	return (mdfs)->file_count * sizeof(mdfs_file_t) + MDFS_EXTRA_CRC_SIZE;
}

inline uint32_t mdfs_get_filecount(mdfs_t* mdfs) __attribute__((always_inline));
inline uint32_t mdfs_get_filecount(mdfs_t* mdfs) { return mdfs->file_count; } 

inline const char* mdfs_get_error(mdfs_t* mdfs) __attribute__((always_inline));
inline const char* mdfs_get_error(mdfs_t* mdfs) { return mdfs->error; }

inline void* mdfs_get_file_location(mdfs_t* mdfs, uint32_t offset) __attribute__((always_inline));
inline void* mdfs_get_file_location(mdfs_t* mdfs, uint32_t offset)
{
	return (void*)(mdfs->target + offset);
}

inline void* mdfs_get_open_file_location(mdfs_FILE* f) __attribute__((always_inline));
inline void* mdfs_get_open_file_location(mdfs_FILE* f)
{
	if (f == NULL) return NULL;
	else return (void*)(f->base + f->offset);
}

inline void* mdfs_get_file_list(mdfs_t* mdfs) __attribute__((always_inline));
inline void* mdfs_get_file_list(mdfs_t* mdfs) { return (void*)mdfs->file_list; }

// CRC functions
#define MDFS_CRC_POLY 0xc9d204f5
uint32_t mdfs_calc_crc(const void* data, int32_t size);
inline uint32_t mdfs_get_stored_crc(mdfs_FILE* f) __attribute__((always_inline));
inline uint32_t mdfs_get_stored_crc(mdfs_FILE* f) { return f->crc; }
int mdfs_check_crc(const mdfs_FILE* f);
int mdfs_set_crc(mdfs_t* mdfs, const char* filename, uint32_t crc);
int mdfs_update_crc(mdfs_t* mdfs, const char* filename);

#endif // _MDFS_H_