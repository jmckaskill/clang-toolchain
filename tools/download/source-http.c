#include "stream.h"
#include "bearssl_wrapper.h"
#include "xz.h"
#include "tar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#define closesocket(fd) close(fd)
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

static int do_read(void *read_context, unsigned char *data, size_t len) {
	int fd = (uintptr_t)read_context;
	return recv(fd, (char*) data, len, 0);
}

static int do_write(void *write_context, const unsigned char *data, size_t len) {
	int fd = (uintptr_t)write_context;
	return send(fd, (const char*) data, len, 0);
}

typedef struct https_stream https_stream;

struct https_stream {
	stream iface;
	br_sslio_context ctx;
	br_ssl_client_context sc;
	br_x509_minimal_context xc;
	int fd, consumed, avail;
	unsigned is_https;
	char *host;
	int64_t length_remaining;
	uint8_t inrec[32 * 1024];
	uint8_t outrec[32 * 1024];
	uint8_t buf[32 * 1024];
};

static uint8_t *https_data(struct stream *s, int *plen, int *atend) {
	https_stream *os = (https_stream*) s;
	*plen = os->avail - os->consumed;
	if ((int64_t) *plen > os->length_remaining) {
		*plen = (int) os->length_remaining;
	}
	if (atend) {
		*atend = (os->length_remaining == 0);
	}
	return os->buf + os->consumed;
}

static void consume_https(struct stream *s, int consumed) {
	https_stream *os = (https_stream*) s;
	os->consumed += consumed;
	os->length_remaining -= consumed;
}

static int download_https(struct stream *s) {
	https_stream *os = (https_stream*) s;
	fprintf(stderr, "download_https %d %d\n", os->avail, os->consumed);
	if (!os->length_remaining) {
		fprintf(stderr, "calling download more after the download is finished");
		return -1;
	}
	if (os->consumed && os->consumed < os->avail) {
		memmove(os->buf, os->buf + os->consumed, os->avail - os->consumed);
	}
	os->avail -= os->consumed;
	os->consumed = 0;
	int r;
	if (os->is_https) {
		r = br_sslio_read(&os->ctx, os->buf + os->avail, sizeof(os->buf) - os->avail);
		if (r <= 0) {
			fprintf(stderr, "ssl error on read %d\n", br_ssl_engine_last_error(os->ctx.engine));
			return -1;
		}
	} else {
		r = recv(os->fd, os->buf + os->avail, sizeof(os->buf) - os->avail, 0);
		if (r <= 0) {
			fprintf(stderr, "error on read\n");
			return -1;
		}
	}
	os->avail += r;
	fprintf(stderr, "downloaded %d\n", r);
	return 0;
}

static char *get_line(struct https_stream *os) {
	for (;;) {
		os->length_remaining = INT64_MAX;
		int bufsz;
		char *line = (char*) https_data(&os->iface, &bufsz, NULL);
		char *nl = memchr(line, '\n', bufsz);
		if (!nl) {
			if (bufsz > 512) {
				fprintf(stderr, "overlong header line\n");
				return NULL;
			}
			if (download_https(&os->iface)) {
				return NULL;
			}
			continue;
		}

		os->consumed = nl + 1 - (char*) os->buf;
		if (nl[-1] == '\r') {
			nl--;
		}
		*nl = 0;
		return line;
	}
}

static struct https_stream gos;

stream *open_http_downloader(const char *url) {
	unsigned is_https;

	if (!strncmp(url, "https://", strlen("https://"))) {
		is_https = 1;
		url += strlen("https://");
	} else if (!strncmp(url, "http://", strlen("http://"))) {
		is_https = 0;
		url += strlen("http://");
	} else {
		return NULL;
	}

	const char *path = strchr(url, '/');
	char *host;
	if (path) {
		host = (char*) malloc(path - url + 1);
		memcpy(host, url, path - url);
		host[path - url] = 0;
	} else {
		host = strdup(url);
		path = "/";
	}

	if (gos.host && !strcmp(host, gos.host) && gos.is_https == is_https) {
		// we can reuse the existing connection
		free(host);
		host = gos.host;
	} else {
		int fd = open_client_socket(host, is_https ? 443 : 80);
		if (fd < 0) {
			fprintf(stderr, "failed to connect to %s\n", host);
			free(host);
			return NULL;
		}
		free(gos.host);
		closesocket(gos.fd);
		gos.host = host;
		gos.fd = fd;

		if (is_https) {
			br_ssl_client_init_full(&gos.sc, &gos.xc, TAs, TAs_NUM);
			br_ssl_engine_set_buffers_bidi(&gos.sc.eng, gos.inrec, sizeof(gos.inrec), gos.outrec, sizeof(gos.outrec));
			br_ssl_client_reset(&gos.sc, host, 0);
			br_sslio_init(&gos.ctx, &gos.sc.eng, &do_read, (void*)(intptr_t)fd, &do_write, (void*)(intptr_t)fd);
		}
	}

	char request[256];
	int reqsz = snprintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost:%s\r\n\r\n", path, host);

	if (is_https) {
		int w = br_sslio_write_all(&gos.ctx, request, reqsz);
		int f = br_sslio_flush(&gos.ctx);
		if (w < 0 || f < 0) {
			fprintf(stderr, "ssl error on write %d\n", br_ssl_engine_last_error(&gos.sc.eng));
			goto err;
		}
	} else {
		char *p = request;
		while (reqsz) {
			int w = send(gos.fd, p, reqsz, 0);
			if (w <= 0) {
				fprintf(stderr, "write error\n");
				goto err;
			}
			reqsz -= w;
		}
	}

	char *hdr = get_line(&gos);
	if (!hdr) {
		goto err;
	}
	char *httpcode = strchr(hdr, ' ');
	if (!httpcode) {
		fprintf(stderr, "invalid header %s\n", hdr);
		goto err;
	}
	int httperr = atoi(httpcode);
	if (httperr / 100 != 2) {
		fprintf(stderr, "http error %d\n", httperr);
		goto err;
	}

	int64_t content_length = -1;

	for (;;) {
		char *p = get_line(&gos);
		if (!p) {
			goto err;
		} else if (*p == 0) {
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
		goto err;
	}

	gos.iface.get_more = &download_https;
	gos.iface.buffered = &https_data;
	gos.iface.consume = &consume_https;
	gos.length_remaining = content_length;
	return &gos.iface;

err:
	closesocket(gos.fd);
	free(gos.host);
	gos.host = NULL;
	return NULL;
}

