/* Most Dumb FileSystem

Readonly filesystem intended for memory mapped flash devices.
No write actions are performed.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "MDFS.h"



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
static int _mdfs_build_file_list(mdfs_t* mdfs);
static int _mdfs_get_file_index(mdfs_t* mdfs, const char* filename);
static mdfs_file_t* _mdfs_alloc_entry(const char* filename, int filesize, uint32_t byte_offset);
static int _mdfs_insert(mdfs_t* mdfs, mdfs_file_t* entry, int index);

#define _MDFS_INCREMENT_FILE_COUNT(mdfs) mdfs->file_list = (mdfs_file_t**)realloc((void*)mdfs->file_list, ++mdfs->file_count * sizeof(mdfs_file_t*))
#define _mdfs_free_entry(entry) free(entry)


/** @brief Returns an initialized mdfs instance
 * 
 * @copybrief MDFS_init_simple
 * We're assuming reading from the device is done directly 
 *
 * @param target Absolute flash address of block 0
 * @ingroup mdfs
 */
mdfs_t* mdfs_init_simple(const void* target) {
	printf("MDFS_init\n");
	mdfs_t* mdfs = (mdfs_t*)malloc(sizeof(mdfs_t));
	mdfs->target = target;
	// array of 8 mdfs_file_t*. 0 initilized:
	mdfs->file_list = NULL; // = (mdfs_file_t**)malloc(0);
	mdfs->file_count = 0;
	memset((void*)mdfs->error, 0, MDFS_ERROR_LEN);
	_mdfs_build_file_list(mdfs);
	return mdfs;
}

/** @brief Build the list of files from block 0
 *
 * @copybrief MDFS_build_file_list
 *
 * @param mdfs Initialized instance of mdfs_t, see @ref MDFS_open_simple.
 * @returns Number of files in the list.
 *
 * @todo discard entry if size or offset don't make sense, or if filename contains non-ascii or doesn't end in \0
 * 
 * @ingroup MDFS
 */
static int _mdfs_build_file_list(mdfs_t* mdfs)
{
	// for 
	// if entry at i size != 0
	//   if index >= file_count
	//     resize file_list
	//   
    //   allocate new file_list entry and memcpy from fs
	printf("MDFS_build_file_list\n");
	int i, j;
	int count = 0;
	for (i = 0; i < MDFS_MAX_FILECOUNT; ++i)
	{
    // Grab the entry from the array in block 0 (which starts at mdfs->target)
    mdfs_file_t* target = &((mdfs_file_t*)mdfs->target)[i];
    // printf("[%i] s=%i, o=0x%08X\n", i, target->size, target->byte_offset);
    // Check sanity of filesize and offset
		if (
      (target->size > 0) &&
      (target->size <= MDFS_MAX_FILESIZE) &&
      (target->byte_offset >= MDFS_BLOCKSIZE)
    )
		{
      // Check filename for non-ascii chars before \0
      int should_skip = 0;
      for (j = 0; j < MDFS_MAX_FILENAME; ++j)
      {
        if (target->filename[j] == 0) break;
        if (!isprint(target->filename[j])) {
          should_skip = 1;
          break;
        }
      }
      // If j reached end, the string wasn't 0 terminated.
      if (j >= MDFS_MAX_FILENAME || j == 0 || should_skip) continue;
      // Everything makes sense, add it to the list
			if (i >= mdfs->file_count) _MDFS_INCREMENT_FILE_COUNT(mdfs);
			mdfs->file_list[count] = (mdfs_file_t*)malloc(sizeof(mdfs_file_t));
			memcpy((void*)mdfs->file_list[count], &((mdfs_file_t*)mdfs->target)[i], sizeof(mdfs_file_t));
			// // Force last char in name \0
			// mdfs->file_list[count]->filename[MDFS_MAX_FILENAME-1] = '\0';
			++count;
		}
	}
	return count;
}

/** @brief Close/Deinitialize mdfs
 * 
 * @ingroup mdfs
 */
void mdfs_deinit(mdfs_t* mdfs)
{
  int i = 0;
  for (i = 0; i < mdfs->file_count; ++i)
  {
    _mdfs_free_entry(mdfs->file_list[i]);
  }
  free(mdfs->file_list);
  free(mdfs);
}

/** @brief Get the filename at index in the file_list
 *
 * @copybrief MDFS_get_filename
 * All indices are filled so if you get 0 chars, you've reached the end of the list.
 *
 * @param mdfs Initialized instance of mdfs_t, see @ref MDFS_open_simple.
 * @param index Index in the file_list
 * @param buffer Target location for the filename. Must be at least @ref MDFS_MAX_FILENAME bytes.
 * @returns The number of characters in the name (strlen). -1 In case there's no file at index,
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

uint32_t mdfs_get_file_offset(mdfs_t* mdfs, int index)
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
 * @remarks This function can recurse up to 511, make sure there is stack space.
 * 
 * @param mdfs Pointer to initialized mdfs.
 * @param filename Name of the new file (will be truncated to MDFS_MAX_FILENAME-1)
 * @param size Size of the file in bytes.
 * @returns The offset from the base of mdfs which is always >= MDFS_BLOCKSIZE in case
 * of success
 *
 * @ingroup mdfs
 */
uint32_t mdfs_add_file(mdfs_t* mdfs, const char* filename, int32_t size)
{
  if (size <= 0) 
  {
    snprintf(mdfs->error, MDFS_ERROR_LEN, "Invalid size");
  	return 0;
  }
   
  int i = 0; // Insertion index in file_list.
  uint32_t target = MDFS_BLOCKSIZE;  // Target byte offset for new file
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
  mdfs_file_t* new = _mdfs_alloc_entry(filename, size, target);
  int error = _mdfs_insert(mdfs, new, i);
  switch (error)
  {
  case -1:
    snprintf(mdfs->error, MDFS_ERROR_LEN, "borked index?");
    _mdfs_free_entry(new);
    return 0;
  case -2:
    snprintf(mdfs->error, MDFS_ERROR_LEN, "No room in file list");
    _mdfs_free_entry(new);
    return 0;
  case 0:
    return target;
  default:
    snprintf(mdfs->error, MDFS_ERROR_LEN, "unknown error: %i", error);
    _mdfs_free_entry(new);
    return 0;
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
 * @remarks Call @ref mdfs_close when you're done
 * 
 * @ingroup mdfs
 */
mdfs_FILE* mdfs_fopen(mdfs_t* mdfs, const char* filename, const char* mode)
{
  // Only allow all mode starting with r
  if (mode[0] != 'r') 
  {
    snprintf(mdfs->error, MDFS_ERROR_LEN, "Unsupported mode: %s", mode);
    errno = EINVAL;
    return NULL;
  }
  if (strcmp(filename, "stdin") == 0)
  {
    mdfs_FILE* fd = malloc(sizeof(mdfs_FILE));
    fd->index = -1;
    return fd;
  }

  int index = _mdfs_get_file_index(mdfs, filename);
  if (index < 0)
  {
    snprintf(mdfs->error, MDFS_ERROR_LEN, "File not found");
    errno = ENOENT;
    return NULL;
  }
  mdfs_FILE* fd = malloc(sizeof(mdfs_FILE));
  // Set values and copy filename
  fd->base = (void*)((uint32_t)mdfs->target + mdfs->file_list[index]->byte_offset);
  fd->offset = 0;
  fd->size = mdfs->file_list[index]->size;
  memcpy((void*)fd->filename, (void*)mdfs->file_list[index]->filename, MDFS_MAX_FILENAME);
  fd->index = index;
  return fd;
}

/** @brief Reopen file with different filename or mode
 * 
 * @ingroup mdfs
 */
mdfs_FILE* mdfs_freopen(mdfs_t* mdfs, const char* filename, const char* mode, mdfs_FILE* f)
{
  if (filename == NULL) filename = f->filename;
  mdfs_FILE* new = mdfs_fopen(mdfs, filename, mode);
  if (new == NULL) 
  {
    mdfs_fclose(f);
    return NULL;
  }
  memcpy((void*)f, (void*)new, sizeof(mdfs_FILE));
  f->offset = 0;
  mdfs_fclose(new);
  return f;
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
 * @param size Size in bytes of each element to be read. LIMITED TO 1
 * @param count Number of elements to read.
 * @param f pointer to a file opened with @ref mdfs_fopen.
 * @returns The number of elements read.
 * 
 * @ingroup mdfs
 */
size_t mdfs_fread(void* ptr, size_t size, size_t count, mdfs_FILE* f)
{
  if (size == 0 || count == 0) return 0;


  // printf("reading %i elements of %i bytes\n", count, size);
  // printf("offset = %i, base = 0x%p, size = %i\n", f->offset, f->base, f->size);

  if (f->offset + count <= f->size) // if we can fit entire count still
  {
    memcpy(ptr, (void*)(f->base + f->offset), count);
    f->offset += count;
    return count;
  }
  else
  {
    // count doesn't fit
    int n = f->size - f->offset;
    if (n < 0) return 0;
    memcpy(ptr, (void*)(f->base + f->offset), n);
    f->offset += n;
    return n;
  }

}

int mdfs_feof(mdfs_FILE* f)
{
  return f->size == f->offset ? 1 : 0;
}

static mdfs_file_t* _mdfs_alloc_entry(const char* filename, int filesize, uint32_t byte_offset)
{
  mdfs_file_t* new_entry = malloc(sizeof(mdfs_file_t));
  snprintf(new_entry->filename, MDFS_MAX_FILENAME, "%s", filename);
  new_entry->size = filesize;
  new_entry->byte_offset = byte_offset;
  return new_entry;
}



int mdfs_fgetc(mdfs_FILE* f)
{
  if (mdfs_feof(f)) {printf("fgetc eof\n"); return MDFS_EOF;}
  return (int)(*(uint8_t*)(f->base + f->offset++));
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




