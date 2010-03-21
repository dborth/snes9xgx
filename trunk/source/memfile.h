/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2010
 *
 * memfile.h
 *
 * Memory-based replacement for standard C file functions
 * This allows standard file calls to be replaced by memory based ones
 * With little modification required to the code
 ***************************************************************************/

#ifndef _MEMFILE_H_
#define _MEMFILE_H_

typedef struct MFile {
	char *buffer; // pointer to buffer memory
	long int offset; // current position indicator in buffer
	long int size; // total file (buffer) size
} MFILE;

#define MSEEK_SET	0
#define MSEEK_CUR	1
#define MSEEK_END	2
#define MEOF		(-1)

MFILE * memfopen(char * buffer, int size);
int memfclose(MFILE * src);
int memfseek ( MFILE * m, long int offset, int origin );
long int memftell (MFILE * stream);
size_t memfread(void * dst, size_t size, size_t count, MFILE * src);
int memfgetc(MFILE * src);

#endif
