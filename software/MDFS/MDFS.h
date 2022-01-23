#ifndef _MDFS_H_
#define _MDFS_H_

#include <stdint.h>
/** @brief Maximum number of chars in a name, \0 included */
#define MDFS_MAX_FILENAME (120)

#define MDFS_BLOCKSIZE (65536)
#define MDFS_MAX_FILECOUNT (512) // = 64 kB block 0 with 128 byte entries
#define MDFS_MAX_FILESIZE (1073741824) // = 1 GB
#define MDFS_ERROR_LEN (80)
#define MDFS_STATE_CLOSED (0)
#define MDFS_STATE_OPEN (1)
#define MDFS_EOF EOF

typedef struct _mdfs_iobuf
{
  int index;
  uint32_t offset;
  void* base;
  int32_t size;
  char filename[MDFS_MAX_FILENAME];
} mdfs_FILE;


typedef struct MDFSFile {
	int32_t size;
	uint32_t byte_offset; ///< From start of FS (yes I don't expect > 4 GB)
	char filename[MDFS_MAX_FILENAME];
} mdfs_file_t;

typedef struct MDFS {
	const void* target;
	mdfs_file_t** file_list; ///< List is ordered by byte_offset
	uint32_t file_count; ///< Number of entries in file_list
	char error[MDFS_ERROR_LEN]; ///< Buffer for error msg. Always a valid string.
} mdfs_t;


mdfs_t* mdfs_init_simple(const void* target);
void mdfs_deinit(mdfs_t* mdfs);
int mdfs_get_filename(mdfs_t* mdfs, int index, char* buffer);
int32_t mdfs_get_filesize(mdfs_t* mdfs, int index);
uint32_t mdfs_get_file_offset(mdfs_t* mdfs, int index);
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
#define mdfs_get_filelist_size(mdfs) ((mdfs)->file_count * sizeof(mdfs_file_t))

inline uint32_t mdfs_get_filecount(mdfs_t* mdfs) __attribute__((always_inline));
inline uint32_t mdfs_get_filecount(mdfs_t* mdfs) { return mdfs->file_count; } 

inline const char* mdfs_get_error(mdfs_t* mdfs) __attribute__((always_inline));
inline const char* mdfs_get_error(mdfs_t* mdfs) { return mdfs->error; }

#endif // _MDFS_H_