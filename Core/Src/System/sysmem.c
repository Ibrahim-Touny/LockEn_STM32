/**
 * @file    sysmem.c
 * @brief   Heap memory management for malloc/free (Newlib).
 *          Implements _sbrk() to grow the heap from the end of .bss
 *          up to the MSP stack boundary.
 */

/* Includes */
#include <errno.h>
#include <stdint.h>

/**
 * Pointer to the current high watermark of the heap usage
 */
static uint8_t *__sbrk_heap_end = NULL;

/**
 * @brief Allocate memory to the heap for malloc() and calloc().
 *
 * Grows the heap from '_end' up to the reserved MSP stack boundary (_estack - _Min_Stack_Size).
 * Called by malloc/calloc internally.
 *
 * @param incr Number of bytes to allocate
 * @return Pointer to allocated memory, or (void*)-1 on failure (heap/stack collision)
 */
void *_sbrk(ptrdiff_t incr)
{
  extern uint8_t _end; /* Symbol defined in the linker script */
  extern uint8_t _estack; /* Symbol defined in the linker script */
  extern uint32_t _Min_Stack_Size; /* Symbol defined in the linker script */
  const uint32_t stack_limit = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
  const uint8_t *max_heap = (uint8_t *)stack_limit;
  uint8_t *prev_heap_end;

  /* Initialize heap end at first call */
  if (NULL == __sbrk_heap_end)
  {
    __sbrk_heap_end = &_end;
  }

  /* Protect heap from growing into the reserved MSP stack */
  if (__sbrk_heap_end + incr > max_heap)
  {
    errno = ENOMEM;
    return (void *)-1;
  }

  prev_heap_end = __sbrk_heap_end;
  __sbrk_heap_end += incr;

  return (void *)prev_heap_end;
}
