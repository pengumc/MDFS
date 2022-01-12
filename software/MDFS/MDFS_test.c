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

static int _test_fread(mdfs_t* mdfs)
{
  // Assume there's a file called 'file1'
  printf("Testing fread\n");
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
  while (!mdfs_feof(f)) 
  {
    result = mdfs_fread((void*)buffer, 1, 13, f);
    buffer[13] = '\0';
    if (result != 13) 
    {
      printf("FAILED (%i)\n", result);
      mdfs_fclose(f);
      return -1;
    }
    printf("fread 13 bytes of file1: %s\n", buffer);
  }

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
  uint32_t result;
  // Error for size 0
  result = mdfs_add_file(mdfs, "add file test 1", 0);
  printf("add file with size 0: (%i) %s\n", result, mdfs->error);
  if (result >= MDFS_BLOCKSIZE || strlen(mdfs->error) <= 0) return -1;
  // Error for size negative
  result = mdfs_add_file(mdfs, "add file test 2", -1);
  printf("add file with size -1: (%i) %s\n", result, mdfs->error);
  if (result >= MDFS_BLOCKSIZE || strlen(mdfs->error) <= 0) return -1;
  // Normal add file call
  result = mdfs_add_file(mdfs, "add file test 3", 1);
  printf("add file with size 1: (0x%08X)\n", result);
  if (result < MDFS_BLOCKSIZE) return -1;
  // Test next file is added after it
  uint32_t last_file = result;
  result = mdfs_add_file(mdfs, "add file test 4", 9);
  printf("add file with size 9: (0x%08X)\n", result);
  if (result != last_file + 1) {
    printf("failed\n");
    return -1;
  }
  // Test next file is added after is with size > 1
  last_file = result;
  result = mdfs_add_file(mdfs, "add file test 4", 3);
  printf("add file with size 3: (0x%08X)\n", result);
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
    // If we're full (i >= filecount) but there's no error, that's a fail
    if (i >= mdfs->file_count && ( result >= MDFS_BLOCKSIZE || strlen(mdfs->error) <= 0 ))
    {
      printf("Failed at %i, file_count = %i, result 0x%08X\n", i, mdfs->file_count, result);
      return -1;
    }
  }
  if (mdfs->file_count != MDFS_MAX_FILECOUNT)
  {
    printf("file_count is not max after trying to add %i files\n", MDFS_MAX_FILECOUNT + 10);
    return -1;
  }
  printf("add files when full: (0x%08X) %s\n", result, mdfs->error);

  return 0; // All passed
}


static int _test_fopen(mdfs_t* mdfs)
{
  printf("Testing fopen\n");
  mdfs_FILE* result;
  // unsupported mode of multiple chars
  result = mdfs_fopen(mdfs, "file1", "wb");
  printf("fopen with mode wb: (0x%p) %s\n", result, mdfs->error);
  if (result != NULL || strlen(mdfs->error) <= 0) return -1;
  // unsupported mode of single chars
  result = mdfs_fopen(mdfs, "file1", "a");
  printf("fopen with mode a: (0x%p) %s\n", result, mdfs->error);
  if (result != NULL || strlen(mdfs->error) <= 0) return -1;
  // non-existing file
  result = mdfs_fopen(mdfs, "plop", "r");
  printf("fopen non-existing file: (0x%p) %s\n", result, mdfs->error);
  if (result != NULL || strlen(mdfs->error) <= 0) return -1;
  // non existing filename longer than allowed
  result = mdfs_fopen(mdfs, "this is a really long filename longer than the allowed 120 ish chars which is actually quite lot now that i'm typing it, damn", "r");
  printf("fopen non-existing long filename: (0x%p) %s\n", result, mdfs->error);
  if (result != NULL || strlen(mdfs->error) <= 0) return -1;
  // Open an actual file
  result = mdfs_fopen(mdfs, "file1", "r");
  printf("fopen file1: (0x%p)\n", result);
  if (result == NULL) return -1;
  printf("  name=%s\n  size=%i\n  offset=%i\n  base=0x%p\n  index=%i\n", 
    result->filename,
    result->size,
    result->offset,
    result->base,
    result->index);
  if (
    (strcmp("file1", result->filename) != 0) ||
    (result->size <= 0) ||
    (result->base < mdfs->target + MDFS_MAX_FILECOUNT * sizeof(mdfs_file_t)) ||
    (result->index != _mdfs_get_file_index(mdfs, "file1"))
  ) 
  {
    mdfs_fclose(result);
    return -1;
  }
  mdfs_fclose(result);

  return 0;
}




void _test_fgetc(mdfs_t* mdfs)
{
  mdfs_FILE* f = mdfs_fopen(mdfs, "file1", "r");
  char buf[512];
  char c = 0;
  int n = 0;
  while (c != EOF && n < 512)
  {
    c = mdfs_fgetc(f);
    buf[n++] = c;
  }
  char buf2[512];
  snprintf(buf2, n+1, "%s", buf);
  printf("read: %s\n", buf2);
  mdfs_fclose(f);
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

/* mdfs_init_simple filecount should be 2 */
static int T_mdfs_init_simple_init_0xFF_expect_filecount_2()
{
  int test_result = 0;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "this is file_A", "this is file_B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  if (mdfs_get_filecount(mdfs) != 2)
  {
    printf("\t[ERROR] T_mdfs_init_simple_init_0xFF_expect_filecount_2: filecount=%i\n", mdfs_get_filecount(mdfs));
    printf("\tFile list:\n");
    _print_file_list(mdfs);
    test_result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

/* mdfs_init_simple filecount should be 2 */
static int T_mdfs_init_simple_init_0x00_expect_filecount_2()
{
  int test_result = 0;
  const void* fs = fs_factory(0x00, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "this is file_A", "this is file_B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  if (mdfs_get_filecount(mdfs) != 2)
  {
    printf("\t[ERROR] T_mdfs_init_simple_init_0x00_expect_filecount_2: filecount=%i\n", mdfs_get_filecount(mdfs));
    printf("\tFile list:\n");
    _print_file_list(mdfs);
    test_result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

/* mdfs_init_simple filecount should be 2 */
// -- This currently fails due to 0x01 being seen as valid filesize
static int T_mdfs_init_simple_init_0x01_expect_filecount_2()
{
  int test_result = 0;
  const void* fs = fs_factory(0x01, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "this is file_A", "this is file_B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  if (mdfs_get_filecount(mdfs) != 2)
  {
    printf("\t[ERROR] T_mdfs_init_simple_init_0x01_expect_filecount_2: filecount=%i\n", mdfs_get_filecount(mdfs));
    printf("\tFile list:\n");
    _print_file_list(mdfs);
    test_result = -1;
  }
  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

/* test fopen to return NULL for non-existing files and give error*/
static int T_mdfs_fopen_non_existing_expect_NULL()
{
  int test_result = 0;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  mdfs_FILE* f;

  f = mdfs_fopen(mdfs, "", "r");
  if (f != NULL) 
  {
    printf("\tfopen(\"\") gives non-NULL: %p\n", f);
    mdfs_fclose(f);
    test_result = -1;
  }

  f = mdfs_fopen(mdfs, "plop", "r");
  if (f != NULL) 
  {
    printf("\tfopen(\"plop\") gives non-NULL: %p\n", f);
    mdfs_fclose(f);
    test_result = -1;
  }


  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

/* Test fopen to return a handle when opening an exiting file */
static int T_mdfs_fopen_existing_expect_ptr()
{
  int test_result = 0;
  const void* fs = fs_factory(0xFF, MDFS_BLOCKSIZE, MDFS_BLOCKSIZE+50, "This is file A", "this is file B");
  mdfs_t* mdfs = mdfs_init_simple(fs);
  mdfs_FILE* f;

  f = mdfs_fopen(mdfs, "file_A", "r");
  if (f == NULL) 
  {
    printf("\tfopen(\"file_A\") gives NULL: %p\n", f);
    test_result = -1;
  }
  mdfs_fclose(f);

  f = mdfs_fopen(mdfs, "file_B", "r");
  if (f == NULL) 
  {
    printf("\tfopen(\"file_B\") gives NULL: %p\n", f);
    test_result = -1;
  }
  mdfs_fclose(f);

  mdfs_deinit(mdfs);
  free((void*)fs);
  return test_result;
}

int main(int argc, char** argv)
{
	// FILE* f = fopen("test_fs", "r");
	// if (f == NULL) 
	// {
	// 	printf("File error\n");
	// 	return 1;
	// }
	// void* test_fs = malloc(65536*2);
	// fread(test_fs, 2, 65536, f);
	// fclose(f);
	// printf("buffer @ %p\n", test_fs);
	// printf("0x%08X\n", ((uint32_t*)test_fs)[0]);
	
	// mdfs_t* mdfs = mdfs_init_simple(test_fs);
	// print_file_list(mdfs);
  // if (_test_fopen(mdfs) == 0) printf("-- fopen: OK\n");
  // else printf("fopen: FAILED\n");
  // if (_test_fread(mdfs) == 0) printf("-- fread: OK\n");
  // else printf("fread: FAILED\n");
  // if (_test_add_file(mdfs) == 0) printf("-- add file: OK\n");
  // else printf("add files: FAILED, %s\n", mdfs->error);

  // // _test_fgetc(mdfs);

  // mdfs_deinit(mdfs);
  // free(test_fs);
  int result;

  result = T_mdfs_init_simple_init_0xFF_expect_filecount_2();
  printf("== T_mdfs_init_simple_init_0xFF_expect_filecount_2: %i ==\n", result);

  result = T_mdfs_init_simple_init_0x00_expect_filecount_2();
  printf("== T_mdfs_init_simple_init_0x00_expect_filecount_2: %i ==\n", result);

  result = T_mdfs_init_simple_init_0x01_expect_filecount_2();
  printf("== T_mdfs_init_simple_init_0x01_expect_filecount_2: %i ==\n", result);

  result = T_mdfs_fopen_non_existing_expect_NULL();
  printf("== T_mdfs_fopen_non_existing_expect_NULL: %i ==\n", result);

  result = T_mdfs_fopen_existing_expect_ptr();
  printf("== T_mdfs_fopen_existing_expect_ptr: %i ==\n", result);

	return 0;
}
