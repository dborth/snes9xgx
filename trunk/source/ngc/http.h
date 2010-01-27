/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2010
 *
 * http.h
 *
 * HTTP operations
 ***************************************************************************/

#ifndef _HTTP_H_
#define _HTTP_H_

#define TCP_CONNECT_TIMEOUT 5000
#define TCP_BLOCK_SIZE (16 * 1024)
#define TCP_BLOCK_RECV_TIMEOUT 4000
#define TCP_BLOCK_SEND_TIMEOUT 4000
#define HTTP_TIMEOUT 300000

typedef enum {
	HTTPR_OK,
	HTTPR_ERR_CONNECT,
	HTTPR_ERR_REQUEST,
	HTTPR_ERR_STATUS,
	HTTPR_ERR_TOOBIG,
	HTTPR_ERR_RECEIVE
} http_res;

bool http_request (const char *url, FILE * hfile, u8 * buffer, const u32 max_size);

#endif
