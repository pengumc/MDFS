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

#define _MDFS_INCREMENT_FILE_COUNT(mdfs) mdfs->file_list = (mdfs_file_t*)realloc((void*)mdfs->file_list, ++mdfs->file_count * sizeof(mdfs_file_t) + MDFS_EXTRA_CRC_SIZE)
#define _MDFS_DECREMENT_FILE_COUNT(mdfs) mdfs->file_list = (mdfs_file_t*)realloc((void*)mdfs->file_list, --mdfs->file_count * sizeof(mdfs_file_t) + MDFS_EXTRA_CRC_SIZE)
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
	mdfs_t* mdfs = (mdfs_t*)malloc(sizeof(mdfs_t));
	mdfs->target = target;
	mdfs->file_list = NULL;
	mdfs->file_count = 0;
	memset((void*)mdfs->error, 0, MDFS_ERROR_LEN);
	_mdfs_build_file_list(mdfs);
	return mdfs;
}

/* Returns 0 whe name is printable, not length 0 or > max */
static int _check_name(const char* name)
{
  int i;
  for (i = 0; i < MDFS_MAX_FILENAME; ++i)
  {
    if (name[i] == 0) break;
    if (!isprint(name[i])) {
      return 1;
    }
  }
  if (i >= MDFS_MAX_FILENAME || i == 0) return 1;
  else return 0;
}

static void _mdfs_update_file_list_crc(mdfs_t* mdfs)
{
  if (mdfs->file_count <= 0) return;
  // size 0 is appended to end of file list to make builder stop
  // next 4 bytes is CRC
  // The space is already allocated
  
  uint32_t crc = mdfs_calc_crc(mdfs->file_list, mdfs->file_count * sizeof(mdfs_file_t));
  uint32_t* p = (uint32_t*)&mdfs->file_list[mdfs->file_count];
  *p++ = 0;
  *p = crc;
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
	int i;
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
      // Check filename for non-ascii chars before \0 or weird length
      if (_check_name(target->filename)) continue;
      // Everything makes sense, add it to the list
			if (i >= mdfs->file_count) _MDFS_INCREMENT_FILE_COUNT(mdfs);
			memcpy((void*)&mdfs->file_list[count], &((mdfs_file_t*)mdfs->target)[i], sizeof(mdfs_file_t));
			// // Force last char in name \0
			// mdfs->file_list[count]->filename[MDFS_MAX_FILENAME-1] = '\0';
			++count;
		}
	}
  _mdfs_update_file_list_crc(mdfs);
	return count;
}

/** @brief Close/Deinitialize mdfs
 * 
 * @todo just free filelist?
 * @ingroup mdfs
 */
void mdfs_deinit(mdfs_t* mdfs)
{
  int i = 0;
  for (i = 0; i < mdfs->file_count; ++i)
  {
    //_mdfs_free_entry(mdfs->file_list[i]);
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
	
	strcpy(buffer, mdfs->file_list[index].filename);
	return strlen(mdfs->file_list[index].filename);
}

int32_t mdfs_get_filesize(mdfs_t* mdfs, int index)
{
	if (index < 0 || index >= mdfs->file_count)
	{
		snprintf(mdfs->error, MDFS_ERROR_LEN, "Invalid index.");
		return -1;
	}
	return mdfs->file_list[index].size;
}

uint32_t mdfs_get_file_offset(mdfs_t* mdfs, int index)
{
	if (index < 0 || index >= mdfs->file_count)
	{
		snprintf(mdfs->error, MDFS_ERROR_LEN, "Invalid index.");
		return -1;
	}
	return mdfs->file_list[index].byte_offset;
}

uint32_t mdfs_get_file_crc(mdfs_t* mdfs, int index)
{
  if (index < 0 || index >= mdfs->file_count)
  {
		snprintf(mdfs->error, MDFS_ERROR_LEN, "Invalid index.");
		return -1;
  }
  return mdfs->file_list[index].crc;
}

/** @brief Add a file to the file list and return it's expected location
 * 
 * @copybrief mdfs_add_file
 * No checks are done on uniqueness of the filename. If the file already exists
 * it is simply created a second time.
 * 
 * Error text is written in case of failure.
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
   
  if (_check_name(filename)) 
  {
    snprintf(mdfs->error, MDFS_ERROR_LEN, "Invalid name");
    return 0;
  }

  int i = 0; // Insertion index in file_list.
  uint32_t target = MDFS_BLOCKSIZE;  // Target byte offset for new file
  mdfs_file_t* next_file = NULL;
  while(1)
  {
    
    if (i < mdfs->file_count) // Check if there's a file at i
    {
      next_file = &mdfs->file_list[i];
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
  _mdfs_free_entry(new);
  switch (error)
  {
  case -1:
    snprintf(mdfs->error, MDFS_ERROR_LEN, "borked index?");
    return 0;
  case -2:
    snprintf(mdfs->error, MDFS_ERROR_LEN, "No room in file list");
    return 0;
  case 0:
    return target;
  default:
    snprintf(mdfs->error, MDFS_ERROR_LEN, "unknown error: %i", error);
    return 0;
  }
}


/** @brief Remove a file from the filelist
 * 
 * @copybrief mdfs_remove_file
 * Removes all occurences of filename from the list.
 * 
 * @returns The number of files removed
 * 
 * @ingroup mdfs
 */
int mdfs_remove_file(mdfs_t* mdfs, const char* filename)
{
  if (_check_name(filename)) return 0;
  // All indices must be populated
  // so after removal we should shift entries around
  int i, j;
  int count = 0;
  for (i = 0; i < mdfs->file_count; ++i)
  {
    if (strcmp(filename, mdfs->file_list[i].filename)) continue; // no match
    // move all remaining entries 
    for (j = i+1; j < mdfs->file_count; ++j)
    {
      memcpy((void*)&mdfs->file_list[j-1], (void*)&mdfs->file_list[j], sizeof(mdfs_file_t));
    }
    // Now decrement filelist
    _MDFS_DECREMENT_FILE_COUNT(mdfs);
    // We removed entry i, so decrement and continue
    --i;
    ++count;
  }
  if (count) _mdfs_update_file_list_crc(mdfs);
  return count;
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
  fd->base = (void*)((uint32_t)mdfs->target + mdfs->file_list[index].byte_offset);
  fd->offset = 0;
  fd->size = mdfs->file_list[index].size;
  fd->crc = mdfs->file_list[index].crc;
  memcpy((void*)fd->filename, (void*)mdfs->file_list[index].filename, MDFS_MAX_FILENAME);
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
 * Works like libc fread, size is forced to 1.
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
  mdfs_file_t* new_entry = calloc(1, sizeof(mdfs_file_t));
  snprintf(new_entry->filename, MDFS_MAX_FILENAME, "%s", filename);
  new_entry->size = filesize;
  new_entry->byte_offset = byte_offset;
  new_entry->crc = 0;
  return new_entry;
}



int mdfs_fgetc(mdfs_FILE* f)
{
  if (mdfs_feof(f)) return MDFS_EOF;
  return (int)(*(uint8_t*)(f->base + f->offset++));
}


static int _mdfs_insert(mdfs_t* mdfs, mdfs_file_t* entry, int index)
{
	if (mdfs->file_count >= MDFS_MAX_FILECOUNT) return -2; // Error: No room
	if (index == mdfs->file_count)
	{
		// New file at end of list, make room and copy entry into it.
		_MDFS_INCREMENT_FILE_COUNT(mdfs);
    memcpy((void*)&mdfs->file_list[index], (void*)entry, sizeof(mdfs_file_t));
    _mdfs_update_file_list_crc(mdfs);
		return 0;
	}
	else if (index > mdfs->file_count)
	{
		// Wut? what are you doing?
		return -1; // Error: Illegal index
	}
	else
	{
		// Insert at existing index. 
    // Make room
		_MDFS_INCREMENT_FILE_COUNT(mdfs);
    // Shift everything down
    // ex: Insert at 1 with file_count 5->6: copy items at 1..5 to 2..6
    // copy to 5 to 6 first, then move back. (we're not betting on cache/memcpy 
    // implementation fixing this for us)
    int i;
    for (i = mdfs->file_count-1; i > index; --i)
    {
      memcpy(
        (void*)&mdfs->file_list[i],
        (void*)&mdfs->file_list[i-1],
        sizeof(mdfs_file_t));
    }
    // Copy entry into index
    memcpy((void*)&mdfs->file_list[index], (void*)entry, sizeof(mdfs_file_t));
    _mdfs_update_file_list_crc(mdfs);
		return 0;
	}
}

/// Returns -1 if file doesn't exist
static int _mdfs_get_file_index(mdfs_t* mdfs, const char* filename)
{
  int i = 0;
  for (i = 0; i < mdfs->file_count; ++i)
  {
    if (strcmp(mdfs->file_list[i].filename, filename) == 0) return i;
  }
  return -1;
}


// ------------------------------------------------------------------

/** @brief Caculate the crc for for data
 * 
 * @copybrief mdfs_calc_crc
 * Polynomial is 0xc9d204f5 (in implicit + 1 notation). 0x93a409eb in wikipedia's
 * normal representation.
 * @param data A pointer to the data to calculate the crc over.
 * @param size The number of bytes in data.
 * @returns the calculated crc. 0xFFFFFFFF in case of invalid sizes or NULL 
 * data.
 * @ingroup mdfs
 */
uint32_t mdfs_calc_crc(const void* data, int32_t size)
{
  // default crc impl. don't care about speed
  // https://wiki.osdev.org/CRC32
  // polynomial = 0x93a409eb (bit per term, x^32 implicit)
  // Below table is generated with included crc32_gen_table.py
  static const uint32_t _table[256] = {
    0x00000000, 0x3CD0EADC, 0x79A1D5B8, 0x45713F64,
    0xF343AB70, 0xCF9341AC, 0x8AE27EC8, 0xB6329414,
    0x49A71D73, 0x7577F7AF, 0x3006C8CB, 0x0CD62217,
    0xBAE4B603, 0x86345CDF, 0xC34563BB, 0xFF958967,
    0x934E3AE6, 0xAF9ED03A, 0xEAEFEF5E, 0xD63F0582,
    0x600D9196, 0x5CDD7B4A, 0x19AC442E, 0x257CAEF2,
    0xDAE92795, 0xE639CD49, 0xA348F22D, 0x9F9818F1,
    0x29AA8CE5, 0x157A6639, 0x500B595D, 0x6CDBB381,
    0x89BC3E5F, 0xB56CD483, 0xF01DEBE7, 0xCCCD013B,
    0x7AFF952F, 0x462F7FF3, 0x035E4097, 0x3F8EAA4B,
    0xC01B232C, 0xFCCBC9F0, 0xB9BAF694, 0x856A1C48,
    0x3358885C, 0x0F886280, 0x4AF95DE4, 0x7629B738,
    0x1AF204B9, 0x2622EE65, 0x6353D101, 0x5F833BDD,
    0xE9B1AFC9, 0xD5614515, 0x90107A71, 0xACC090AD,
    0x535519CA, 0x6F85F316, 0x2AF4CC72, 0x162426AE,
    0xA016B2BA, 0x9CC65866, 0xD9B76702, 0xE5678DDE,
    0xBC58372D, 0x8088DDF1, 0xC5F9E295, 0xF9290849,
    0x4F1B9C5D, 0x73CB7681, 0x36BA49E5, 0x0A6AA339,
    0xF5FF2A5E, 0xC92FC082, 0x8C5EFFE6, 0xB08E153A,
    0x06BC812E, 0x3A6C6BF2, 0x7F1D5496, 0x43CDBE4A,
    0x2F160DCB, 0x13C6E717, 0x56B7D873, 0x6A6732AF,
    0xDC55A6BB, 0xE0854C67, 0xA5F47303, 0x992499DF,
    0x66B110B8, 0x5A61FA64, 0x1F10C500, 0x23C02FDC,
    0x95F2BBC8, 0xA9225114, 0xEC536E70, 0xD08384AC,
    0x35E40972, 0x0934E3AE, 0x4C45DCCA, 0x70953616,
    0xC6A7A202, 0xFA7748DE, 0xBF0677BA, 0x83D69D66,
    0x7C431401, 0x4093FEDD, 0x05E2C1B9, 0x39322B65,
    0x8F00BF71, 0xB3D055AD, 0xF6A16AC9, 0xCA718015,
    0xA6AA3394, 0x9A7AD948, 0xDF0BE62C, 0xE3DB0CF0,
    0x55E998E4, 0x69397238, 0x2C484D5C, 0x1098A780,
    0xEF0D2EE7, 0xD3DDC43B, 0x96ACFB5F, 0xAA7C1183,
    0x1C4E8597, 0x209E6F4B, 0x65EF502F, 0x593FBAF3,
    0xD79025C9, 0xEB40CF15, 0xAE31F071, 0x92E11AAD,
    0x24D38EB9, 0x18036465, 0x5D725B01, 0x61A2B1DD,
    0x9E3738BA, 0xA2E7D266, 0xE796ED02, 0xDB4607DE,
    0x6D7493CA, 0x51A47916, 0x14D54672, 0x2805ACAE,
    0x44DE1F2F, 0x780EF5F3, 0x3D7FCA97, 0x01AF204B,
    0xB79DB45F, 0x8B4D5E83, 0xCE3C61E7, 0xF2EC8B3B,
    0x0D79025C, 0x31A9E880, 0x74D8D7E4, 0x48083D38,
    0xFE3AA92C, 0xC2EA43F0, 0x879B7C94, 0xBB4B9648,
    0x5E2C1B96, 0x62FCF14A, 0x278DCE2E, 0x1B5D24F2,
    0xAD6FB0E6, 0x91BF5A3A, 0xD4CE655E, 0xE81E8F82,
    0x178B06E5, 0x2B5BEC39, 0x6E2AD35D, 0x52FA3981,
    0xE4C8AD95, 0xD8184749, 0x9D69782D, 0xA1B992F1,
    0xCD622170, 0xF1B2CBAC, 0xB4C3F4C8, 0x88131E14,
    0x3E218A00, 0x02F160DC, 0x47805FB8, 0x7B50B564,
    0x84C53C03, 0xB815D6DF, 0xFD64E9BB, 0xC1B40367,
    0x77869773, 0x4B567DAF, 0x0E2742CB, 0x32F7A817,
    0x6BC812E4, 0x5718F838, 0x1269C75C, 0x2EB92D80,
    0x988BB994, 0xA45B5348, 0xE12A6C2C, 0xDDFA86F0,
    0x226F0F97, 0x1EBFE54B, 0x5BCEDA2F, 0x671E30F3,
    0xD12CA4E7, 0xEDFC4E3B, 0xA88D715F, 0x945D9B83,
    0xF8862802, 0xC456C2DE, 0x8127FDBA, 0xBDF71766,
    0x0BC58372, 0x371569AE, 0x726456CA, 0x4EB4BC16,
    0xB1213571, 0x8DF1DFAD, 0xC880E0C9, 0xF4500A15,
    0x42629E01, 0x7EB274DD, 0x3BC34BB9, 0x0713A165,
    0xE2742CBB, 0xDEA4C667, 0x9BD5F903, 0xA70513DF,
    0x113787CB, 0x2DE76D17, 0x68965273, 0x5446B8AF,
    0xABD331C8, 0x9703DB14, 0xD272E470, 0xEEA20EAC,
    0x58909AB8, 0x64407064, 0x21314F00, 0x1DE1A5DC,
    0x713A165D, 0x4DEAFC81, 0x089BC3E5, 0x344B2939,
    0x8279BD2D, 0xBEA957F1, 0xFBD86895, 0xC7088249,
    0x389D0B2E, 0x044DE1F2, 0x413CDE96, 0x7DEC344A,
    0xCBDEA05E, 0xF70E4A82, 0xB27F75E6, 0x8EAF9F3A};

  uint32_t crc = 0xffffffff;
  if (size <= 0 || data == NULL) return crc;
	while (size-- !=0)
  {
    crc = _table[(((uint8_t) crc) ^ *(uint8_t*)(data++))] ^ (crc >> 8);
  } 
	return (crc ^ 0xffffffff);
}


/** @brief Read file and check crc
 * 
 * @copybrief mdfs_check_crc
 * The calculated value is checked against the one in the filehandle.
 * @param f An opened file handle. The file will be read but
 * the handle will remain unchanged
 * @returns 1 when values match. 0 otherwise.
 * @ingroup mdfs
 */
int mdfs_check_crc(const mdfs_FILE* f)
{
  void* data = f->base + f->offset;
  uint32_t crc = mdfs_calc_crc(data, f->size);
  if (crc == f->crc) return 1;
  else return 0;
}


/** @brief Set the crc value for a file in the file list
 * 
 * @copybrief mdfs_set_crc
 * The crc is stored in the file list. After a change you'll have to update the
 * file list on disk.
 * @param mdfs The mdfs 
 * @param filename The filename
 * @param crc The new crc value to set
 * @returns 0 on success, -1 when file doesn't exist
 * @ingroup mdfs
 */
int mdfs_set_crc(mdfs_t* mdfs, const char* filename, uint32_t crc)
{
  int i = _mdfs_get_file_index(mdfs, filename);
  if (i < 0) return -1;
  mdfs->file_list[i].crc = crc;
  return 0;
}


/** @brief Update the crc for a file
 * 
 * @copybrief mdfs_update_crc
 * The crc in the file list will be updated on success. After a change you'll 
 * have to update the file list on disk.
 * @param mdfs The mdfs
 * @param filename The filename
 * @returns -1 when file doesn't exist. 0 otherwise
 */
int mdfs_update_crc(mdfs_t* mdfs, const char* filename)
{
  mdfs_FILE* f = mdfs_fopen(mdfs, filename, "r");
  if (f == NULL) return -1;
  mdfs->file_list[f->index].crc = mdfs_calc_crc(
    (void*)(f->base + f->offset),
    f->size);
  return 0;
}
