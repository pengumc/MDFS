#ifndef _MDFS_MEM_H_

#include <stdlib.h>
#ifdef _FREERTOS_
#include "FreeRTOS.h"
#include "task.h"

#define MDFS_FREE(x) vPortFree(x)
#define MDFS_MALLOC(x) pvPortMalloc(x)

inline void* MDFS_REALLOC(void* x, size_t y)
{
  vTaskSuspendAll();
  void* ptr = realloc(x, y);
  xTaskResumeAll();
  return ptr;
}                              
#else

#define MDFS_FREE(x) free(x)
#define MDFS_MALLOC(x) malloc(x)
#define MDFS_REALLOC(x, y) realloc(x, y)
#endif


#endif // _MDFS_MEM_H_
