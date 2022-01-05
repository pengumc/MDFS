/* Most Dumb FileSystem

Readonly filesystem intended for memory mapped flash devices.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// fopen
// fclose
// fread
// feof
// getc?
// freopen
// ferror

/**
 * @code
 * mdfs_t* mdfs = MDFS_init_simple(FLASH_BASE_ADDR + 0x02000000);
 * int number_of_files = MDFS_build_file_list(mdfs);
 * 
 * mdfs_fd* my_file = MDFS_open("my_filename", 'r')
 * char buf[512];
 * int chars_read = MDFS_read(my_file, buf, 512);
 * MDFS_close(my_file);
 *
 *
 *
 *
 *
 *
 *
 *
 *
 * @endcode
 */

#define MDFS_MAX_FILENAME (120)
#define MDFS_MAX_FILECOUNT (512) // = 64 kB block 0 with 128 byte entries
#define MDFS_ERROR_LEN (80)
#define MDFS_STATE_CLOSED (0)
#define MDFS_STATE_OPEN (1)

typedef struct _mdfs_iobuf
{
  int index;
  uint32_t offset;
  void* base;
  int32_t size;
  char* filename[MDFS_MAX_FILENAME];
} mdfs_FILE;


typedef struct MDFSFile {
	int32_t size;
	uint32_t byte_offset; ///< From start of FS (yes I don't expect > 4 GB)
	char filename[MDFS_MAX_FILENAME];
} mdfs_file_t;

typedef struct MDFS {
	void* target;
	mdfs_file_t** file_list; ///< List is ordered by byte_offset
	uint32_t file_count; ///< Number of entries in file_list
	char error[MDFS_ERROR_LEN]; ///< Buffer for error msg. Always a valid string.
} mdfs_t;

int mdfs_build_file_list(mdfs_t* mdfs);
static int _mdfs_get_file_index(mdfs_t* mdfs, const char* filename);
static mdfs_file_t* _mdfs_alloc_entry(const char* filename, int filesize, uint32_t byte_offset);
static int _mdfs_insert(mdfs_t* mdfs, mdfs_file_t* entry, int index);
#define _MDFS_INCREMENT_FILE_COUNT(mdfs) mdfs->file_list = (mdfs_file_t**)realloc((void*)mdfs->file_list, ++mdfs->file_count * sizeof(mdfs_file_t*))

/** @brief Returns an initialized mdfs instance
 * 
 * @copybrief MDFS_init_simple
 * We're assuming reading from the device is done directly 
 *
 * @param target Absolute flash address of block 0
 * @ingroup mdfs
 */
mdfs_t* mdfs_init_simple(void* target) {
	printf("MDFS_init\n");
	mdfs_t* mdfs = (mdfs_t*)malloc(sizeof(mdfs_t));
	mdfs->target = target;
	// array of 8 mdfs_file_t*. 0 initilized:
	mdfs->file_list = NULL; // = (mdfs_file_t**)malloc(0);
	mdfs->file_count = 0;
	memset((void*)mdfs->error, 0, MDFS_ERROR_LEN);
	mdfs_build_file_list(mdfs);
	return mdfs;
}

/** @brief Build the list of files from block 0
 *
 * @copybrief MDFS_build_file_list
 *
 * @param mdfs Initialized instance of mdfs_t, see @ref MDFS_open_simple.
 * @returns Number of files in the list.
 *
 * @ingroup MDFS
 */
int mdfs_build_file_list(mdfs_t* mdfs)
{
	// for 
	// if entry at i size != 0
	//   if index >= file_count
	//     resize file_list
	//   
    //   allocate new file_list entry and memcpy from fs
	printf("MDFS_build_file_list\n");
	int i = 0;
	int count = 0;
	for (i = 0; i < MDFS_MAX_FILECOUNT; ++i)
	{
    // Read size of file from block 0
		int32_t apparant_size =  *(int32_t*)( &((mdfs_file_t*)mdfs->target)[i] );
		// printf("%i - size %i\n", i, apparant_size);
		if (apparant_size > 0)
		{
			if (i >= mdfs->file_count) _MDFS_INCREMENT_FILE_COUNT(mdfs);
			mdfs->file_list[count] = (mdfs_file_t*)malloc(sizeof(mdfs_file_t));
			memcpy((void*)mdfs->file_list[count], &((mdfs_file_t*)mdfs->target)[i], sizeof(mdfs_file_t));
			// Force last char in name \0
			mdfs->file_list[count]->filename[MDFS_MAX_FILENAME-1] = '\0';
			++count;
		}
	}
	return count;
}

/** @brief Get the filename at index in the file_list
 *
 * @copybrief MDFS_get_filename
 * All indices are filled so if you get 0 chars, you've reached the end of the list.
 *
 * @param mdfs Initialized instance of mdfs_t, see @ref MDFS_open_simple.
 * @param index Index in the file_list
 * @param buffer Target location for the filename. Must be at least @ref MDFS_MAX_FILENAME bytes.
 * @returns The number of characters copied to buffer. -1 In case there's no file at index,
 * mdfs->error is written in that case.
 *
 * @ingroup MDFS
 */
int mdfs_get_filename(mdfs_t* mdfs, int index, char* buffer)
{
	if (index < 0 || index >= mdfs->file_count)
	{
		snprintf(mdfs->error, MDFS_ERROR_LEN, "Invalid index.");
		return -1;
	}
	
	strcpy(buffer, mdfs->file_list[index]->filename);
	return strlen(mdfs->file_list[index]->filename);
}

int32_t mdfs_get_filesize(mdfs_t* mdfs, int index)
{
	if (index < 0 || index >= mdfs->file_count)
	{
		snprintf(mdfs->error, MDFS_ERROR_LEN, "Invalid index.");
		return -1;
	}
	return mdfs->file_list[index]->size;
}

uint32_t mdfs_get_file_location(mdfs_t* mdfs, int index)
{
	if (index < 0 || index >= mdfs->file_count)
	{
		snprintf(mdfs->error, MDFS_ERROR_LEN, "Invalid index.");
		return -1;
	}
	return mdfs->file_list[index]->byte_offset;
}

/** @brief Add a file to the file list and return it's expected location
 * 
 * @copybrief mdfs_add_file
 * No checks are done on uniqueness of the filename. If the file already exists
 * it is simply created a second time.
 *
 * @param mdfs Pointer to initialized mdfs.
 * @param filename Name of the new file (will be truncated to MDFS_MAX_FILENAME-1)
 * @param size Size of the file in bytes.
 * @returns A pointer to the target location for this file.
 *
 * @ingroup mdfs
 */
void* mdfs_add_file(mdfs_t* mdfs, const char* filename, int32_t size)
{
  if (size <= 0) 
  {
    snprintf(mdfs->error, MDFS_ERROR_LEN, "Invalid size");
  	return NULL;
  }
   
  int i = 0; // Insertion index in file_list.
  uint32_t target = 0;  // Target byte offset for new file
  mdfs_file_t* next_file = NULL;
  while(1)
  {
    
    if (i < mdfs->file_count) // Check if there's a file at i
    {
      next_file = mdfs->file_list[i];
      if (target + size < next_file->byte_offset) // Is there room before it?
      {
        break;
      }
      else
      {
        // No room
        target = next_file->byte_offset + next_file->size;
        ++i;
        continue;
      }
    }
    else // We're at the end of the list
    {
      break;
    }
  }
  int error = _mdfs_insert(mdfs, _mdfs_alloc_entry(filename, size, target), i);
  switch (error)
  {
  case -1:
    snprintf(mdfs->error, MDFS_ERROR_LEN, "borked index?");
    return NULL;
  case -2:
    snprintf(mdfs->error, MDFS_ERROR_LEN, "No room in file list");
    return NULL;
  case 0:
    return (void*)(mdfs->target + target);
  default:
    snprintf(mdfs->error, MDFS_ERROR_LEN, "unknown error: %i", error);
    return NULL;
  }
}


/** @brief Open a file
 * 
 * @copybrief mdfs_fopen
 * Works like libc fopen
 * 
 * @param mdfs Initialized mdfs.
 * @param filename Filename to open (case sensitive)
 * @param mode Only "r" supported
 * 
 * @remarks Call @ref mdfs_clse when you're done
 * 
 * @ingroup mdfs
 */
mdfs_FILE* mdfs_fopen (mdfs_t* mdfs, const char* filename, const char* mode)
{
  int index = _mdfs_get_file_index(mdfs, filename);
  if (index < 0)
  {
    snprintf(mdfs->error, MDFS_ERROR_LEN, "File not found");
    return NULL;
  }
  mdfs_FILE* fd = malloc(sizeof(mdfs_FILE));
  fd->base = (void*)((uint32_t)mdfs->target + mdfs->file_list[index]->byte_offset);
  fd->offset = 0;
  fd->size = mdfs->file_list[index]->size;
  memcpy((void*)fd->filename, (void*)mdfs->file_list[index], MDFS_MAX_FILENAME);
  fd->index = index;
  return fd;
}


/** @brief close a file previously opened with @ref mdfs_fopen
 * 
 * @copybrief mdfs_fclose
 * 
 * @param f Opened file
 * 
 * @ingroup mdfs
 */
int mdfs_fclose(mdfs_FILE* f)
{
  free(f);
  return 0;
}


/** @brief Read block of data from file
 * 
 * @copybrief mdfs_fread
 * Works like libc fread
 * 
 * @param ptr Pointer to block of memory with at least (size*count) bytes.
 * @param size Size in bytes of each element to be read.
 * @param count Number of elements to read.
 * @param f pointer to a file opened with @ref mdfs_fopen.
 * @returns The number of elements read.
 * 
 * @ingroup mdfs
 */
size_t mdfs_fread(void* ptr, size_t size, size_t count, mdfs_FILE* f)
{
  if (size == 0 || count == 0) return 0;
  printf("reading %i elements of %i bytes\n", count, size);
  printf("offset = %i, base = 0x%p, size = %i\n", f->offset, f->base, f->size);

  size_t read = 0;
  for (read = 0; read < count; ++read)
  {
    // if current offset + size  file size break
    if (f->offset + size > f->size)
    {
      printf("can't read no more, %i + %i > %i\n", f->offset, size, f->size);
      break;
    } 
    memcpy(ptr+read*size, (void*)(f->base + f->offset), size);
    f->offset += size;
  }
  // Don't read more even there's stuff left and room in the buffer
  return read;
}

static mdfs_file_t* _mdfs_alloc_entry(const char* filename, int filesize, uint32_t byte_offset)
{
  mdfs_file_t* new_entry = malloc(sizeof(mdfs_file_t));
  snprintf(new_entry->filename, MDFS_MAX_FILENAME, "%s", filename);
  new_entry->size = filesize;
  new_entry->byte_offset = byte_offset;
  return new_entry;
}

static int _mdfs_insert(mdfs_t* mdfs, mdfs_file_t* entry, int index)
{
	// This function can recurse up to MDFS_MAX_FILECOUNT times.
	// Make sure there is stack space.
	
	if (mdfs->file_count >= MDFS_MAX_FILECOUNT) return -2; // Error: No room
	if (index == mdfs->file_count)
	{
		// New file at end of list
		_MDFS_INCREMENT_FILE_COUNT(mdfs);
		mdfs->file_list[index] = entry;
		return 0;
	}
	else if (index > mdfs->file_count)
	{
		// Wut? what are you doing?
		return -1; // Error: Illegal index
	}
	else
	{
		// Insert at existing index. shift everything down
		mdfs_file_t* temp_entry = mdfs->file_list[index];
		mdfs->file_list[index] = entry;
		return _mdfs_insert(mdfs, temp_entry, index+1);
	}
}


static int _mdfs_get_file_index(mdfs_t* mdfs, const char* filename)
{
  int i = 0;
  for (i = 0; i < mdfs->file_count; ++i)
  {
    if (strcmp(mdfs->file_list[i]->filename, filename) == 0) return i;
  }
  return -1;
}


// ------------------------------------------------------------------
void print_file_list(mdfs_t* mdfs)
{
	int i = 0;
	char* buf = malloc(MDFS_MAX_FILENAME);
	printf("File list:\n");
	while(1)
	{
		if (mdfs_get_filename(mdfs, i, buf) > 0) 
		{
			printf("%i: %s (%i bytes) @ 0x%08X\n", i, buf, mdfs_get_filesize(mdfs, i), mdfs_get_file_location(mdfs, i));
		}
		else
		{
			break;
		}
		++i;
	}
}

static int _test_fread(mdfs_t* mdfs)
{
  // Assume there's a file called 'file1'

  // Open
  mdfs_FILE* f = mdfs_fopen(mdfs, "file1", "r");
  printf("fopen 'file1': (0x%p)\n", f);
  if (f == NULL) 
  {
    printf("FAILED, %s\n", mdfs->error);
    return -1;
  }
  // Read and print entire file in small steps
  char buffer[14];
  int result = 0;
  result = mdfs_fread((void*)buffer, 1, 13, f);
  buffer[13] = '\0';
  if (result != 13) 
  {
    printf("FAILED (%i)\n", result);
    return -1;
  }
  printf("fread 13 bytes of file1: %s\n", buffer);

  // Close file
  result = mdfs_fclose(f);
  if (result != 0) return -1;
  printf("Close file: OK\n");

  return 0;
}


static int _test_add_file(mdfs_t* mdfs)
{
  // Assume file list is empty.

  printf("Testing add file\n");
  void* result;
  // Error for size 0
  result = mdfs_add_file(mdfs, "add file test 1", 0);
  printf("add file with size 0: (NULL) %s\n", mdfs->error);
  if (result != NULL || strlen(mdfs->error) <= 0) return -1;
  // Error for size negative
  result = mdfs_add_file(mdfs, "add file test 2", -1);
  printf("add file with size -1: (NULL) %s\n", mdfs->error);
  if (result != NULL || strlen(mdfs->error) <= 0) return -1;
  // Normal add file call
  result = mdfs_add_file(mdfs, "add file test 3", 1);
  printf("add file with size 1: (0x%p)\n", result);
  if (result == NULL) return -1;
  // Test next file is added after it
  void* last_file = result;
  result = mdfs_add_file(mdfs, "add file test 4", 9);
  printf("add file with size 9: (0x%p)\n", result);
  if (result != last_file + 1) {
    printf("failed\n");
    return -1;
  }
  // Test next file is added after is with size > 1
  last_file = result;
  result = mdfs_add_file(mdfs, "add file test 4", 3);
  printf("add file with size 3: (0x%p)\n", result);
  if (result != last_file + 9) {
    printf("failed\n");
    return -1;
  }
  // Add files till we're full
  int i = 0;
  char namebuf[MDFS_MAX_FILENAME];
  for (i = 0; i < MDFS_MAX_FILECOUNT + 10; ++i)
  {
    snprintf(namebuf, MDFS_MAX_FILENAME, "testfile %i", i);
    result = mdfs_add_file(mdfs, namebuf, 33);
    if (i >= mdfs->file_count && ( result != NULL || strlen(mdfs->error) <= 0 ))
    {
      printf("Failed at %i, file_count = %i, result 0x%p\n", i, mdfs->file_count, result);
      return -1;
    }
  }
  if (mdfs->file_count != MDFS_MAX_FILECOUNT)
  {
    printf("file_count is not max after trying to add %i files\n", MDFS_MAX_FILECOUNT + 10);
    return -1;
  }
  printf("add files when full: (NULL) %s\n", mdfs->error);

  return 0; // All passed
}

int main()
{
	FILE* f = fopen("test_fs", "r");
	if (f == NULL) 
	{
		printf("File error\n");
		return 1;
	}
	void* test_fs = malloc(65536*2);
	fread(test_fs, 2, 65536, f);
	fclose(f);
	printf("buffer @ %p\n", test_fs);
	printf("0x%08X\n", ((uint32_t*)test_fs)[0]);
	
	mdfs_t* mdfs = mdfs_init_simple(test_fs);
	print_file_list(mdfs);
  if (_test_fread(mdfs) == 0) printf("-- fread: OK\n");
  if (_test_add_file(mdfs) == 0) printf("-- add file: OK\n");
  else printf("add files: FAILED, %s\n", mdfs->error);

	// don't care, leak everything
	return 0;
}