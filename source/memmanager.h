/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * memmanager.h
 *
 * Memory manager
 ***************************************************************************/

#ifndef _MEMMANAGER_H_
#define _MEMMANAGER_H_

#if defined(HW_RVL) || defined(HW_DOL)
#define IMAGE_BUFFER_SIZE (640 * 480 * 4)
#define IMAGE_DECODE_SCRATCH_SIZE (IMAGE_BUFFER_SIZE + (480 * sizeof(void*)))
#else
#define IMAGE_BUFFER_SIZE (1920 * 1080 * 4)
#define IMAGE_DECODE_SCRATCH_SIZE (IMAGE_BUFFER_SIZE + (1080 * sizeof(void*)))
#endif
#define PNG_FILE_BUFFER_SIZE (512 * 1024)

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t * romPtr;

void InitMemManager();
void SwitchMemoryModeMenu();
void SwitchMemoryModeGame();
void* extmem_malloc(uint32_t size);
char* extmem_strdup(const char *s);
void extmem_free(void *ptr);
int extmem_size_free();

#ifdef __cplusplus
}
#endif

#endif
