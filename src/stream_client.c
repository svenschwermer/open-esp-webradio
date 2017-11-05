#include "fifo.h"
#include "stream_client.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include <string.h>
#include <unistd.h>

static ssize_t send_http_request(int sock, const char *host, const char *path)
{
	const char *req[] = {
			"GET ",
			path,
			" HTTP/1.0\r\nHost: ",
			host,
			"\r\n\r\n"
	};

	ssize_t written_total = 0;
	for(int i=0; i < sizeof(req)/sizeof(req[0]); ++i)
	{
		const size_t len = strlen(req[i]);
		const ssize_t written = write(sock, req[i], len);

		written_total += written;

		if(len != written)
			return -1;
	}

	return written_total;
}

static enum {
	INIT,
	CR,
	CRLF,
	CRLFCR,
	CONTENT,
} response_parser_state = INIT;

// Quick & Dirty HTTP header parser, returns the number of leading header bytes
static int process_response_header(const char *buf, int len)
{
	if (response_parser_state == CONTENT)
		return 0;

	for (int i=0; i<len; ++i) {
		if (buf[i] == '\r') {
			switch (response_parser_state) {
			case INIT:
				response_parser_state = CR;
				break;
			case CRLF:
				response_parser_state = CRLFCR;
				break;
			default:
				response_parser_state = CONTENT;
				return i;
			}
		} else if (buf[i] == '\n') {
			switch (response_parser_state) {
			case CR:
				response_parser_state = CRLF;
				break;
			case CRLFCR:
				response_parser_state = CONTENT;
				return i+1;
			default:
				response_parser_state = CONTENT;
				return i;
			}
		} else if (buf[i] >= 0x20 && buf[i] <= 0x7e) {
			response_parser_state = INIT;
		} else {
			response_parser_state = CONTENT;
			return i;
		}
	}
	return len;
}

static uint32_t total_bytes = 0;

uint32_t reset_total_bytes()
{
	uint32_t bytes = total_bytes;
	total_bytes = 0;
	return bytes;
}

void stream_task(void *arg)
{
	struct stream_params *params = (struct stream_params *) arg;

	const struct addrinfo hints = {
			.ai_family = AF_INET,
			.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *res;

	printf("Running DNS lookup for %s...\n", params->host);
	int err = getaddrinfo(params->host, "80", &hints, &res);
	if(err != 0 || res == NULL)
	{
		printf("DNS lookup failed err=%d res=%p\r\n", err, res);
		if(res)
			freeaddrinfo(res);
		goto fail;
	}

	/* Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
	struct in_addr *addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
	printf("DNS lookup succeeded. IP=%s\r\n", inet_ntoa(*addr));

	int s = socket(res->ai_family, res->ai_socktype, 0);
	if(s < 0)
	{
		printf("... Failed to allocate socket.\r\n");
		freeaddrinfo(res);
		goto fail;
	}

	printf("... allocated socket\r\n");

	if(connect(s, res->ai_addr, res->ai_addrlen) != 0)
	{
		freeaddrinfo(res);
		printf("... socket connect failed.\r\n");
		goto fail_close_socket;
	}

	printf("... connected\r\n");
	freeaddrinfo(res);

	if (send_http_request(s, params->host, params->path) <= 0)
	{
		printf("... sending http request failed\r\n");
		goto fail_close_socket;
	}
	printf("... http request send success\r\n");

	int n;
	char buf[65];
	while((n = read(s, buf, sizeof buf - 1)) > 0)
	{
		int header_bytes = process_response_header(buf, n);
		if (header_bytes < n) {
			fifo_enqueue(buf + header_bytes, n - header_bytes);
			total_bytes += n - header_bytes;
		}
		if (header_bytes > 0) {
			buf[header_bytes] = '\0';
			printf("%s", buf);
		}
	}

fail_close_socket:
	close(s);
fail:
	vTaskDelete(NULL);
}
