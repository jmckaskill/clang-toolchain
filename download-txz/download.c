#include "bearssl_wrapper.h"
#include "xz.h"
#include "tar.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#endif

#ifdef _MSC_VER
#define strcasecmp(a,b) _stricmp(a,b)
#endif

#include "google-ca.h"

static int open_client_socket(const char *host, int port) {
#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

	struct addrinfo hints, *result, *rp;
	int fd = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* TCP socket */

	char ports[32];
	sprintf(ports, "%d", port);

	if (getaddrinfo(host, ports, &hints, &result)) {
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd == -1) {
			continue;
		}

		if (!connect(fd, rp->ai_addr, rp->ai_addrlen)) {
			break;
		}

		closesocket(fd);
	}

	freeaddrinfo(result);
	return rp ? fd : -1;
}

int do_read(void *read_context, unsigned char *data, size_t len) {
	int fd = (uintptr_t)read_context;
	return recv(fd, (char*) data, len, 0);
}

int do_write(void *write_context, const unsigned char *data, size_t len) {
	int fd = (uintptr_t)write_context;
	return send(fd, (const char*) data, len, 0);
}

struct download_stream {
	br_sslio_context ctx;
	uint8_t inrec[32 * 1024];
	uint8_t outrec[32 * 1024];
	uint8_t buf[32 * 1024];
	int used, avail;
	int64_t length_remaining;
};

uint8_t *downloaded_data(struct download_stream *os, int *plen) {
	*plen = os->avail - os->used;
	if ((int64_t) *plen > os->length_remaining) {
		*plen = (int) os->length_remaining;
	}
	return os->buf + os->used;
}

int download_finished(struct download_stream *os) {
	return os->length_remaining == 0;
}

void download_used(struct download_stream *os, int consumed) {
	os->used += consumed;
	os->length_remaining -= consumed;
}

int download_more(struct download_stream *os) {
	if (!os->length_remaining) {
		return 1;
	}
	if (os->used && os->used < os->avail) {
		memmove(os->buf, os->buf + os->used, os->avail - os->used);
	}
	os->avail -= os->used;
	os->used = 0;
	int r = br_sslio_read(&os->ctx, os->buf + os->avail, sizeof(os->buf) - os->avail);
	if (r < 0) {
		fprintf(stderr, "ssl error on read %d\n", br_ssl_engine_last_error(os->ctx.engine));
		return -1;
	}
	os->avail += r;
	return 0;
}

static char *get_line(struct download_stream *os) {
	for (;;) {
		os->length_remaining = INT64_MAX;
		int bufsz;
		char *line = (char*) downloaded_data(os, &bufsz);
		char *nl = memchr(line, '\n', bufsz);
		if (!nl) {
			if (bufsz > 512) {
				fprintf(stderr, "overlong header line\n");
				return NULL;
			}
			if (download_more(os)) {
				return NULL;
			}
			continue;
		}

		os->used = nl + 1 - (char*) os->buf;
		if (nl[-1] == '\r') {
			nl--;
		}
		*nl = 0;
		return line;
	}
}

static struct download_stream gos;

struct download_stream *open_download_stream(const char *host, const char *path) {
	br_ssl_client_context sc;
	br_x509_minimal_context xc;

	int fd = open_client_socket(host, 443);
	if (fd < 0) {
		fprintf(stderr, "failed to connect to %s\n", host);
		return NULL;
	}

	br_ssl_client_init_full(&sc, &xc, TAs, TAs_NUM);
	br_ssl_engine_set_buffers_bidi(&sc.eng, gos.inrec, sizeof(gos.inrec), gos.outrec, sizeof(gos.outrec));
	br_ssl_client_reset(&sc, "storage.googleapis.com", 0);
	br_sslio_init(&gos.ctx, &sc.eng, &do_read, (void*)(intptr_t)fd, &do_write, (void*)(intptr_t)fd);

	char request[256];
	int reqsz = snprintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost:%s\r\n\r\n", path, host);
	int w = br_sslio_write_all(&gos.ctx, request, reqsz);
	int f = br_sslio_flush(&gos.ctx);
	if (w < 0 || f < 0) {
		fprintf(stderr, "ssl error on write %d\n", br_ssl_engine_last_error(&sc.eng));
		return NULL;
	}

	char *hdr = get_line(&gos);
	if (!hdr) {
		return NULL;
	}
	char *httpcode = strchr(hdr, ' ');
	if (!httpcode) {
		fprintf(stderr, "invalid header %s\n", hdr);
		return NULL;
	}
	int httperr = atoi(httpcode);
	if (httperr / 100 != 2) {
		fprintf(stderr, "http error %d\n", httperr);
		return NULL;
	}

	int64_t content_length = -1;

	for (;;) {
		char *p = get_line(&gos);
		if (!p) {
			return NULL;
		}
		if (*p == 0) {
			break;
		}
		char *colon = strchr(p, ':');
		if (!colon) {
			continue;
		}
		*colon = 0;

		if (!strcasecmp(p, "content-length")) {
			content_length = strtoll(colon + 1, NULL, 0);
		}
	}

	if (content_length < 0) {
		fprintf(stderr, "content length not specified\n");
		return NULL;
	}

	gos.length_remaining = content_length;
	return &gos;
}

