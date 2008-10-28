/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric October 2008
 *
 * memfile.cpp
 *
 * Memory-based replacement for standard C file functions
 * This allows standard file calls to be replaced by memory based ones
 * With little modification required to the code
 ***************************************************************************/
#include <string.h>
#include <stdlib.h>

#include "memfile.h"

/****************************************************************************
 * memfopen
 * memory replacement for fopen
 *
 * Creates a memory-based file
 ***************************************************************************/
MFILE * memfopen(char * buffer, int size)
{
	MFILE *f = (MFILE *)malloc(sizeof(MFILE));

	f->buffer = buffer;
	f->offset = 0;
	f->size = size;

	return f;
}

/****************************************************************************
 * memfclose
 * memory replacement for fclose
 *
 * 'Closes' the memory file specified
 ***************************************************************************/
int memfclose(MFILE * src)
{
	free(src);
	return 0;
}

/****************************************************************************
 * memfseek
 * memory replacement for fseek
 *
 * Sets the position indicator associated with the stream to a new position
 * defined by adding offset to a reference position specified by origin.
 ***************************************************************************/
int memfseek ( MFILE * m, long int offset, int origin )
{
	int success = 0; // 0 indicates success

	switch(origin)
	{
		case MSEEK_SET:
			if(offset >= 0 && offset <= m->size)
				m->offset = offset;
			else
				success = 1; // failure
			break;
		case MSEEK_CUR:
			if((m->offset + offset) >= 0 && (m->offset + offset) <= m->size)
				m->offset += offset;
			else
				success = 1; // failure
			break;
		case MSEEK_END:
			if((m->size + offset) >= 0 && (m->size + offset) <= m->size)
				m->offset = m->size + offset;
			else
				success = 1; // failure
			break;
	}
	return success;
}

/****************************************************************************
 * memftell
 * memory replacement for ftell
 *
 * Get current position in stream (offset + 1)
 ***************************************************************************/
long int memftell (MFILE * stream)
{
	return stream->offset; // to emulate ftell behavior
}

/****************************************************************************
 * memfread
 * memory replacement for fread
 *
 * Reads an array of count elements, each one with a size of size bytes, from
 * the buffer and stores them in the block of memory specified by ptr. The
 * postion indicator of the buffer is advanced by the total amount of bytes
 * read. The total amount of bytes read if successful is (size * count).
 ***************************************************************************/
size_t memfread(void * dst, size_t size, size_t count, MFILE * src)
{
	if(src->offset >= src->size) // reached end of buffer
		return 0;

	int numbytes = size*count;

	if((src->offset + numbytes) > src->size) // can't read full # requested
		numbytes = src->size - src->offset; // do a partial read

	if(numbytes > 0)
		memcpy(dst, src->buffer+src->offset, numbytes);

	src->offset += numbytes;
	return numbytes;
}

/****************************************************************************
 * memfgetc
 * memory replacement for fgetc
 *
 * Returns the next character in the buffer specified, and advances the
 * postion indicator of the buffer by 1.
 ***************************************************************************/
int memfgetc(MFILE * src)
{
	if(src->offset >= src->size) // reached end of buffer
		return MEOF;
	else
		return src->buffer[src->offset++];
}
