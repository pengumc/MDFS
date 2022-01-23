#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "MDFS.h"

int _mdfs_get_file_index(mdfs_t* mdfs, const char* filename);

void print_file_list(mdfs_t* mdfs)
{
	int i = 0;
	char* buf = malloc(MDFS_MAX_FILENAME);
	printf("File list:\n");
	while(1)
	{
		if (mdfs_get_filename(mdfs, i, buf) > 0) 
		{
			printf("%i: %s (%i bytes) @ 0x%08X\n", i, buf, mdfs_get_filesize(mdfs, i), mdfs_get_file_offset(mdfs, i));
		}
		else
		{
			break;
		}
		++i;
	}
  free(buf);
}

/* Return a filesystem of 3 blocks for testing.
 * Contains 2 files, "file_A" and "file_B" on offsets A and B 
 * contents of the files are string A and B (null char not included)
 * Unused space is set to 0xFF
 * use free() to deallocate when done
 * You will get NULL if not everything fits into 3 blocks
 */
static const void* fs_factory(int init, uint32_t offset_A, uint32_t offset_B, const char* string_A, const char* string_B)
{
  int len_A = strlen(string_A);
  int len_B = strlen(string_B);
  if ((offset_A + len_A > 3*MDFS_BLOCKSIZE-1) || (offset_B + len_B > 3*MDFS_BLOCKSIZE-1))
  {
    printf("fs_factory: Invalid params");
    return NULL;
  }
  void* fs = malloc(3*MDFS_BLOCKSIZE);
  memset(fs, init, 3*MDFS_BLOCKSIZE);
  void* p = fs;
  // Setup file list
  ((uint32_t*)p)[0] = len_A; // Size
  ((uint32_t*)p)[1] = offset_A; // Offset
  sprintf(p+8, "file_A");
  p += 128;
  ((uint32_t*)p)[0] = len_B; // Size
  ((uint32_t*)p)[1] = offset_B; // Offset
  sprintf(p+8, "file_B");
  p = fs + offset_A;
  memcpy(p, (void*)string_A, len_A); // strlen does not include \0
  p = fs + offset_B;
  memcpy(p, (void*)string_B, len_B);
  return fs;
}

/* Return a filesystem of 3 blocks, filled with init & 0xFF */
static const void* fs_empty(int init)
{
  void* fs = malloc(3*MDFS_BLOCKSIZE);
  memset(fs, init, 3*MDFS_BLOCKSIZE);
  return fs;
}

static void _print_file_list(mdfs_t* mdfs)
{
  int i, len, j;
  char name[MDFS_MAX_FILENAME];
  for (i = 0; i < mdfs_get_filecount(mdfs); ++i)
  {
    printf("\t[%i] size: %i, offset: %i, name:\"",
      i, 
      mdfs_get_filesize(mdfs, i),
      mdfs_get_file_offset(mdfs, i));
    len = mdfs_get_filename(mdfs, i, name);
    for (j=0; j < len; ++j) {
      if (isprint(name[j])) printf("%c", name[j]);
      else printf("\\x%02X", name[j]);
    }
    printf("\"\n");
  }
}

// --------------------------------------------------------------------
// mdfs_init_simple
// --------------------------------------------------------------------
/* mdfs_init_simple filecount should be 2 */
static int T_mdfs_init_simple_init_0xFF_expect_filecount_2()
{
  printf("T_mdfs_init_simple_init_0xFF_expect_filecount_2: ");
  int test_result = 0;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "this is file_A", "this is file_B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  if (mdfs_get_filecount(mdfs) != 2)
  {
    printf("FAILED (filecount=%i)\n", mdfs_get_filecount(mdfs));
    _print_file_list(mdfs);
    test_result = -1;
  } else printf("OK\n");
  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

/* mdfs_init_simple filecount should be 2 */
static int T_mdfs_init_simple_init_0x00_expect_filecount_2()
{
  printf("T_mdfs_init_simple_init_0x00_expect_filecount_2: ");
  int test_result = 0;
  const void* fs = fs_factory(0x00, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "this is file_A", "this is file_B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  if (mdfs_get_filecount(mdfs) != 2)
  {
    printf("FAILED (filecount=%i)\n", mdfs_get_filecount(mdfs));
    _print_file_list(mdfs);
    test_result = -1;
  } else printf("OK\n");
  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

/* mdfs_init_simple filecount should be 2 */
// -- This currently fails due to 0x01 being seen as valid filesize
static int T_mdfs_init_simple_init_0x01_expect_filecount_2()
{
  printf("T_mdfs_init_simple_init_0x01_expect_filecount_2: ");
  int test_result = 0;
  const void* fs = fs_factory(0x01, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "this is file_A", "this is file_B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  if (mdfs_get_filecount(mdfs) != 2)
  {
    printf("FAILED (filecount=%i)\n", mdfs_get_filecount(mdfs));
    _print_file_list(mdfs);
    test_result = -1;
  }
  else printf("OK\n");
  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

static int T_mdfs_init_simple_empty_fileblock_expect_0_files()
{
  int result = -1;
  const void* fs = fs_empty(0xff);
  mdfs_t* mdfs = mdfs_init_simple(fs);
  printf("T_mdfs_init_simple_empty_fileblock_expect_0_files: mdfs = %p ", mdfs);
  uint32_t count = mdfs_get_filecount(mdfs);
  if (count == 0)
  {
    printf("OK\n");
    result = 0;
  }
  else
  {
    printf("FAILED (filecount = %i)\n", count);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_init_simple()
{
  return
    T_mdfs_init_simple_empty_fileblock_expect_0_files() |
    T_mdfs_init_simple_init_0xFF_expect_filecount_2() |
    T_mdfs_init_simple_init_0x00_expect_filecount_2() |
    T_mdfs_init_simple_init_0x01_expect_filecount_2();
}

// --------------------------------------------------------------------
// mdfs_fopen
// --------------------------------------------------------------------
/* test fopen to return NULL for non-existing files and give error*/
static int T_mdfs_fopen_non_existing_expect_NULL()
{
  printf("T_mdfs_fopen_non_existing_expect_NULL: ");
  int test_result = 0;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  mdfs_FILE* f;

  f = mdfs_fopen(mdfs, "", "r");
  if (f != NULL) 
  {
    printf("FAILED (fopen('') = %p)\n", f);
    mdfs_fclose(f);
    test_result = -1;
  }

  f = mdfs_fopen(mdfs, "plop", "r");
  if (f != NULL) 
  {
    printf("FAILED (fopen non-existing = %p)\n", f);
    mdfs_fclose(f);
    test_result = -1;
  }
  if (test_result == 0) printf("OK\n");
  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

/* Test fopen to return a handle when opening an exiting file */
static int T_mdfs_fopen_existing_expect_ptr()
{
  printf("T_mdfs_fopen_existing_expect_ptr: ");
  int test_result = 0;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  mdfs_FILE* f;

  f = mdfs_fopen(mdfs, "file_A", "r");
  if (f == NULL) 
  {
    printf("FAILED (fopen('file_A') = %p)\n", f);
    test_result = -1;
  }
  mdfs_fclose(f);

  f = mdfs_fopen(mdfs, "file_B", "r");
  if (f == NULL) 
  {
    printf("FAILED (fopen('file_B') = %p)\n", f);
    test_result = -1;
  }
  mdfs_fclose(f);
  if (test_result == 0) printf("OK\n");
  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

int T_mdfs_fopen()
{
  return
    T_mdfs_fopen_non_existing_expect_NULL() |
    T_mdfs_fopen_existing_expect_ptr();
}

// --------------------------------------------------------------------
// mdfs_add_file
// --------------------------------------------------------------------
int T_mdfs_add_file_size_0_expect_error()
{
  int result = -1;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  uint32_t offset = mdfs_add_file(mdfs, "test0", 0);
  printf("T_mdfs_add_file_size_0_expect_error: offset = 0x%08X ", offset);
  if (offset < MDFS_BLOCKSIZE)
  {
    printf(" OK\n");
    result = 0;
  }
  else
  {
    printf("FAILED\n");
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_add_file_negative_size_expect_error()
{
  int result = -1;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  uint32_t offset = mdfs_add_file(mdfs, "test0", -1);
  printf("T_mdfs_add_file_negative_size_expect_error: offset = 0x%08X ", offset);
  if (offset < MDFS_BLOCKSIZE)
  {
    printf(" OK\n");
    result = 0;
  }
  else
  {
    printf("FAILED\n");
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_add_file_unprintable_name_expect_error()
{
  int result = -1;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  char name[3] = {11, 21, 0}; // everything under 33 is unprintable
  uint32_t offset = mdfs_add_file(mdfs, name, 10);
  printf("T_mdfs_add_file_unprintable_name_expect_error: offset = 0x%08X ", offset);
  if (offset < MDFS_BLOCKSIZE)
  {
    printf(" OK\n");
    result = 0;
  }
  else
  {
    printf("FAILED\n");
    print_file_list(mdfs);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_add_file_namelen_0_expect_error()
{
  int result = -1;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  char name[1] = {0}; 
  uint32_t offset = mdfs_add_file(mdfs, name, 10);
  printf("T_mdfs_add_file_namelen_0_expect_error: offset = 0x%08X ", offset);
  if (offset < MDFS_BLOCKSIZE)
  {
    printf(" OK\n");
    result = 0;
  }
  else
  {
    printf("FAILED\n");
    print_file_list(mdfs);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_add_file_long_name_expect_error()
{
  int result = -1;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  char name[MDFS_MAX_FILENAME+10];
  int i;
  for(i = 0; i < MDFS_MAX_FILENAME+10; ++i) { name[i] = 'a'; }
  uint32_t offset = mdfs_add_file(mdfs, name, 10);
  printf("T_mdfs_add_file_long_name_expect_error: offset = 0x%08X ", offset);
  if (offset < MDFS_BLOCKSIZE)
  {
    printf(" OK\n");
    result = 0;
  }
  else
  {
    printf("FAILED\n");
    print_file_list(mdfs);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_add_file_to_empty_list_expect_success()
{
  int result = -1;
  const void* fs = fs_empty(0x03);
  mdfs_t* mdfs = mdfs_init_simple(fs);
  uint32_t offset = mdfs_add_file(mdfs, "plop", 10);
  printf("T_mdfs_add_file_to_empty_list_expect_success: offset = 0x%08X ", offset);
  if (offset >= MDFS_BLOCKSIZE)
  {
    printf(" OK\n");
    result = 0;
  }
  else
  {
    printf("FAILED (%s)\n", mdfs_get_error(mdfs));
    print_file_list(mdfs);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_add_file_to_full_list_expect_error()
{
  printf("T_mdfs_add_file_to_full_list_expect_error: ");
  int result = -1;
  const void* fs = fs_empty(0x03);
  mdfs_t* mdfs = mdfs_init_simple(fs);
  // Fill with MDFS_MAX_FILECOUNT files
  char name[MDFS_MAX_FILENAME];
  uint32_t offset;
  int i;
  for(i = 0; i < MDFS_MAX_FILECOUNT; ++i)
  {
    sprintf(name, "%i", i);
    offset = mdfs_add_file(mdfs, name, 1);
  }
  uint32_t count = mdfs_get_filecount(mdfs);
  if (count != MDFS_MAX_FILECOUNT)
  {
    printf("FAILED (failed to fill, filecount = %i)\n", count);
    result = -1;
  }
  else
  {
    offset = mdfs_add_file(mdfs, "testfile", 1);
    
    printf("offset = 0x%08X ", offset);
    if (offset < MDFS_BLOCKSIZE)
    {
      printf(" OK\n");
      result = 0;
    }
    else
    {
      printf("FAILED (%s)\n", mdfs_get_error(mdfs));
      print_file_list(mdfs);
      result = -1;
    }
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_add_file()
{
  return 
  T_mdfs_add_file_size_0_expect_error() |
  T_mdfs_add_file_negative_size_expect_error() |
  T_mdfs_add_file_unprintable_name_expect_error() |
  T_mdfs_add_file_namelen_0_expect_error() |
  T_mdfs_add_file_long_name_expect_error() |
  T_mdfs_add_file_to_empty_list_expect_success() |
  T_mdfs_add_file_to_full_list_expect_error();
}

// --------------------------------------------------------------------
// mdfs_remove_file
// --------------------------------------------------------------------
int T_mdfs_remove_file_nonexisting_expect_0()
{
  // Try to remove files not in the list
  printf("T_mdfs_remove_file_nonexisting_expect_0: ");
  int result = -1;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  uint32_t filecount = mdfs_get_filecount(mdfs);
  int count = mdfs_remove_file(mdfs, "file_C");
  if ((count == 0) && (mdfs_get_filecount(mdfs) == filecount))
  {
    printf("OK\n");
    result = 0;
  }
  else 
  {
    printf("FAILED (removed %i file(s), filecount = %i)\n", count, mdfs_get_filecount(mdfs));
    print_file_list(mdfs);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_remove_file_unique_expect_1()
{
  // Try to remove an existing unique file from the list
  printf("T_mdfs_remove_file_unique_expect_1: ");
  int result = -1;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  uint32_t filecount = mdfs_get_filecount(mdfs);
  int count = mdfs_remove_file(mdfs, "file_A");
  if ((count == 1) && (mdfs_get_filecount(mdfs) == filecount - 1))
  {
    printf("OK\n");
    result = 0;
  }
  else 
  {
    printf("FAILED (removed %i file(s), filecount = %i)\n", count, mdfs_get_filecount(mdfs));
    print_file_list(mdfs);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_remove_file_duplicates_expect_3()
{
  // Try to remove 3 duplicate names at once
  printf("T_mdfs_remove_file_duplicates_expect_3: ");
  int result = -1;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  mdfs_add_file(mdfs, "file_A", 10);
  mdfs_add_file(mdfs, "file_A", 10);
  // There should be 3 file_A now
  uint32_t filecount = mdfs_get_filecount(mdfs);
  int count = mdfs_remove_file(mdfs, "file_A");
  if ((count == 3) && (mdfs_get_filecount(mdfs) == filecount - 3))
  {
    printf("OK\n");
    result = 0;
  }
  else 
  {
    printf("FAILED (removed %i file(s), filecount = %i)\n", count, mdfs_get_filecount(mdfs));
    print_file_list(mdfs);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_remove_file_from_full_list_expect_1()
{
 // Try to remove a unique file from a full list
  printf("T_mdfs_remove_file_from_full_list_expect_1: ");
  int result = -1;
  const void* fs = fs_empty(0);
  mdfs_t* mdfs = mdfs_init_simple(fs);
  mdfs_add_file(mdfs, "test_file", 1);
  int i;
  for (i = 0; i < MDFS_MAX_FILECOUNT-1; ++i) { mdfs_add_file(mdfs, "plop", 1); }
  // There should be MAX files now, with test_file at the start of the list
  int count = mdfs_remove_file(mdfs, "test_file");
  if ((count == 1) && (mdfs_get_filecount(mdfs) == MDFS_MAX_FILECOUNT - 1))
  {
    printf("OK\n");
    result = 0;
  }
  else 
  {
    printf("FAILED (removed %i file(s), filecount = %i)\n", count, mdfs_get_filecount(mdfs));
    print_file_list(mdfs);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_remove_file_from_emtpy_list_expect_0()
{
 // Try to remove a non-existing file from an empty list
  printf("T_mdfs_remove_file_from_emtpy_list_expect_0: ");
  int result = -1;
  const void* fs = fs_empty(0);
  mdfs_t* mdfs = mdfs_init_simple(fs);
  // There should be no files
  int count = mdfs_remove_file(mdfs, "test_file");
  if ((count == 0) && (mdfs_get_filecount(mdfs) == 0))
  {
    printf("OK\n");
    result = 0;
  }
  else 
  {
    printf("FAILED (removed %i file(s), filecount = %i)\n", count, mdfs_get_filecount(mdfs));
    print_file_list(mdfs);
    result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return result;
}

int T_mdfs_remove_file()
{
  return 
    T_mdfs_remove_file_duplicates_expect_3() |
    T_mdfs_remove_file_from_emtpy_list_expect_0() |
    T_mdfs_remove_file_from_full_list_expect_1() |
    T_mdfs_remove_file_nonexisting_expect_0() |
    T_mdfs_remove_file_unique_expect_1();
}

// --------------------------------------------------------------------
int main(int argc, char** argv)
{
  int result = T_mdfs_init_simple();
  result |= T_mdfs_fopen();
  result |= T_mdfs_add_file();
  result |= T_mdfs_remove_file();
  printf("\n == %s ==\n", result ? "FAILED" : "PASSED");
  return result;
}
