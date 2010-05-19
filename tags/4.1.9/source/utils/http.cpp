/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric December 2008
 *
 * http.cpp
 *
 * HTTP operations
 * Written by dhewg/bushing, modified by Tantric
 ***************************************************************************/

#ifdef HW_RVL

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ogcsys.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <fcntl.h>

#include "menu.h"
#include "http.h"

static s32 tcp_socket(void)
{
	s32 s, res;

	s = net_socket(PF_INET, SOCK_STREAM, 0);
	if (s < 0)
	{

		return s;
	}

	res = net_fcntl(s, F_GETFL, 0);
	if (res < 0)
	{

		net_close(s);
		return res;
	}

	res = net_fcntl(s, F_SETFL, res | 4);
	if (res < 0)
	{

		net_close(s);
		return res;
	}

	return s;
}

static s32 tcp_connect(char *host, const u16 port)
{
	struct hostent *hp;
	struct sockaddr_in sa;
	s32 s, res;
	s64 t;

	hp = net_gethostbyname(host);
	if (!hp || !(hp->h_addrtype == PF_INET))
	{

		return errno;
	}

	s = tcp_socket();
	if (s < 0)
		return s;

	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = PF_INET;
	sa.sin_len = sizeof(struct sockaddr_in);
	sa.sin_port = htons(port);
	memcpy((char *) &sa.sin_addr, hp->h_addr_list[0], hp->h_length);

	t = gettime();
	while (true)
	{
		if (ticks_to_millisecs(diff_ticks(t, gettime())) > TCP_CONNECT_TIMEOUT)
		{

			net_close(s);

			return -ETIMEDOUT;
		}

		res = net_connect(s, (struct sockaddr *) &sa,
				sizeof(struct sockaddr_in));

		if (res < 0)
		{
			if (res == -EISCONN)
				break;

			if (res == -EINPROGRESS || res == -EALREADY)
			{
				usleep(20 * 1000);

				continue;
			}

			net_close(s);

			return res;
		}

		break;
	}

	return s;
}

static char * tcp_readln(const s32 s, const u16 max_length, const u64 start_time,
		const u16 timeout)
{
	char *buf;
	u16 c;
	s32 res;
	char *ret;

	buf = (char *) malloc(max_length);

	c = 0;
	ret = NULL;
	while (true)
	{
		if (ticks_to_millisecs(diff_ticks(start_time, gettime())) > timeout)
			break;

		res = net_read(s, &buf[c], 1);

		if ((res == 0) || (res == -EAGAIN))
		{
			usleep(20 * 1000);

			continue;
		}

		if (res < 0)
		{

			break;
		}

		if ((c > 0) && (buf[c - 1] == '\r') && (buf[c] == '\n'))
		{
			if (c == 1)
			{
				ret = strdup("");

				break;
			}

			ret = strndup(buf, c - 1);

			break;
		}

		c++;

		if (c == max_length)
			break;
	}

	free(buf);
	return ret;
}

static int tcp_read(const s32 s, u8 **buffer, const u32 length)
{
	u8 *p;
	u32 step, left, block, received;
	s64 t;
	s32 res;

	step = 0;
	p = *buffer;
	left = length;
	received = 0;

	t = gettime();
	while (left)
	{
		if (ticks_to_millisecs(diff_ticks(t, gettime()))
				> TCP_BLOCK_RECV_TIMEOUT)
		{
			break;
		}

		block = left;
		if (block > 2048)
			block = 2048;

		res = net_read(s, p, block);

		if ((res == 0) || (res == -EAGAIN))
		{
			usleep(20 * 1000);

			continue;
		}

		if (res < 0)
		{
			break;
		}

		received += res;
		left -= res;
		p += res;

		if ((received / TCP_BLOCK_SIZE) > step)
		{
			t = gettime();
			step++;
		}
	}

	return left == 0;
}

static int tcp_write(const s32 s, const u8 *buffer, const u32 length)
{
	const u8 *p;
	u32 step, left, block, sent;
	s64 t;
	s32 res;

	step = 0;
	p = buffer;
	left = length;
	sent = 0;

	t = gettime();
	while (left)
	{
		if (ticks_to_millisecs(diff_ticks(t, gettime()))
				> TCP_BLOCK_SEND_TIMEOUT)
		{

			break;
		}

		block = left;
		if (block > 2048)
			block = 2048;

		res = net_write(s, p, block);

		if ((res == 0) || (res == -56))
		{
			usleep(20 * 1000);
			continue;
		}

		if (res < 0)
		{

			break;
		}

		sent += res;
		left -= res;
		p += res;

		if ((sent / TCP_BLOCK_SIZE) > step)
		{
			t = gettime();
			step++;
		}
	}

	return left == 0;
}
static bool http_split_url(char **host, char **path, const char *url)
{
	const char *p;
	char *c;

	if (strncasecmp(url, "http://", 7))
		return false;

	p = url + 7;
	c = strchr(p, '/');

	if (c[0] == 0)
		return false;

	*host = strndup(p, c - p);
	*path = strdup(c);

	return true;
}

/****************************************************************************
 * http_request
 * Retrieves the specified URL, and stores it in the specified file or buffer
 ***************************************************************************/
bool http_request(const char *url, FILE * hfile, u8 * buffer,
		const u32 max_size)
{
	int res = 0;
	char *http_host;
	u16 http_port;
	char *http_path;

	http_res result;
	u32 http_status;
	u32 content_length = 0;

	int linecount;
	if (!http_split_url(&http_host, &http_path, url))
		return false;

	if (hfile == NULL && buffer == NULL)
		return false;

	http_port = 80;
	http_status = 404;

	int s = tcp_connect(http_host, http_port);

	if (s < 0)
	{
		result = HTTPR_ERR_CONNECT;
		return false;
	}

	char *request = (char *) malloc(1024);
	char *r = request;
	r += sprintf(r, "GET %s HTTP/1.1\r\n", http_path);
	r += sprintf(r, "Host: %s\r\n", http_host);
	r += sprintf(r, "Cache-Control: no-cache\r\n\r\n");

	res = tcp_write(s, (u8 *) request, strlen(request));

	free(request);

	for (linecount = 0; linecount < 32; linecount++)
	{
		char *line = tcp_readln(s, 0xff, gettime(), (const u16) HTTP_TIMEOUT);

		if (!line)
		{
			http_status = 404;
			result = HTTPR_ERR_REQUEST;
			break;
		}

		if (strlen(line) < 1)
		{
			free(line);
			line = NULL;
			break;
		}

		sscanf(line, "HTTP/1.%*u %u", &http_status);
		sscanf(line, "Content-Length: %u", &content_length);

		free(line);
		line = NULL;

	}

	if (linecount == 32 || !content_length)
		http_status = 404;
	if (http_status != 200)
	{
		result = HTTPR_ERR_STATUS;
		net_close(s);
		return false;
	}
	if (content_length > max_size)
	{
		result = HTTPR_ERR_TOOBIG;
		net_close(s);
		return false;
	}

	if (buffer != NULL)
	{
		res = tcp_read(s, &buffer, content_length);
	}
	else
	{
		// read into file
		u32 bufSize = (1026 * 256);
		u32 bytesLeft = content_length;
		u32 readSize;

		ShowProgress("Downloading...", 0, content_length);
		u8 * fbuffer = (u8 *) malloc(bufSize);
		if(fbuffer)
		{
			while (bytesLeft > 0)
			{
				if (bytesLeft < bufSize)
					readSize = bytesLeft;
				else
					readSize = bufSize;

				res = tcp_read(s, &fbuffer, readSize);
				if (!res)
					break;
				res = fwrite(fbuffer, 1, readSize, hfile);
				if (!res)
					break;

				bytesLeft -= readSize;
				ShowProgress("Downloading...", (content_length - bytesLeft),
						content_length);
			}
			free(fbuffer);
		}
		CancelAction();
	}

	net_close(s);

	if (!res)
	{
		result = HTTPR_ERR_RECEIVE;
		return false;
	}

	result = HTTPR_OK;
	return true;
}
#endif
