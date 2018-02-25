#include "stream_client.h"
#include "common.h"
#include "fifo.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/netdb.h"

#include "lwip/dns.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "espressif/esp_common.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static const char *stream_host;
static const char *stream_path;
static stream_up_cb up_cb;
static stream_metadata_cb metadata_cb;
static bool stop;
static TaskHandle_t handle;

static ssize_t send_http_request(int sock, const char *host, const char *path) {
  const char *req[] = {"GET ", path, " HTTP/1.0\r\nHost: ", host,
                       "\r\nIcy-MetaData: 1\r\n\r\n"};

  ssize_t written_total = 0;
  for (int i = 0; i < ARRAY_SIZE(req); ++i) {
    const size_t len = strlen(req[i]);
    const ssize_t written = write(sock, req[i], len);

    written_total += written;

    if (len != written)
      return -1;
  }

  return written_total;
}

// Parses the HTTP reply. Returns 0 on success.
// Parses the metadata interval as advertised by the server via the
// 'icy-metaint' header attribute. The result is written into the variable
// pointed to by metaint. If the attribute cannot be found, the variable is set
// to -1. metaint may be set to NULL. The payload read after the header is
// enqueued into the FIFO and the length of said payload is written into the
// variable pointed to by metapos.
static int read_header(int socket, int *metaint, int *metapos) {
  int ret = 1;
  static const size_t buffer_size = 2048;
  char *buffer = malloc(buffer_size);
  if (buffer == NULL) {
    printf("allocating header buffer failed\n");
    goto out;
  }
  memset(buffer, 0, buffer_size);

  int n = read(socket, buffer, buffer_size - 1);
  if (n <= 0) {
    printf("receiving header lines failed\n");
    goto free_buffer;
  }

  char *header_end = strstr(buffer, "\r\n\r\n");
  if (header_end == NULL) {
    printf("header too long\n");
    goto free_buffer;
  }
  header_end[2] = '\0';
  header_end += 4;

  const char *status_code_pos = strchr(buffer, ' ');
  if (status_code_pos == NULL || strncmp(status_code_pos + 1, "200", 3) != 0) {
    *strstr(buffer, "\r\n") = '\0';
    printf("invalid reply status: %s\n", buffer);
    goto free_buffer;
  }

  puts(buffer);

  if (metaint != NULL) {
    const char *metaint_pos = strstr(buffer, "icy-metaint:");
    if (metaint_pos != NULL && metaint_pos < header_end) {
      char *line_end = strstr(metaint_pos, "\r\n");
      *line_end = '\0';
      *metaint = atoi(metaint_pos + 12);
    } else {
      *metaint = -1;
    }
  }

  int header_len = header_end - buffer;
  fifo_enqueue(header_end, n - header_len);
  if (metapos != NULL)
    *metapos = n - header_len;

  ret = 0;

free_buffer:
  free(buffer);
out:
  return ret;
}

static void parse_metadata(const char *s, int len) {
  static enum { INIT, OTHER, ARTIST, TITLE } state = INIT;
  static char buf[64];
  static int buf_pos = 0;

  while (len > 0) {
    int available = sizeof(buf) - buf_pos;
    if (available == 0) {
      // If we run of out buffer space, we can't loop forever. If we're in the
      // middle of parsing the artist or the title, we just termiate the string
      // and use the incomplete string. That's probably better than skipping it
      // altogether.
      buf[sizeof(buf) - 1] = '\0';
      switch (state) {
      case ARTIST:
        metadata_cb(STREAM_ARTIST, buf);
        break;
      case TITLE:
        metadata_cb(STREAM_TITLE, buf);
        break;
      default:
        break;
      }
      state = INIT;
      available = sizeof(buf);
      buf_pos = 0;
    }
    int n = min(len, available);
    memcpy(buf + buf_pos, s, n);
    buf_pos += n;
    s += n;
    len -= n;

    // get rid of trailing zero bytes
    while (buf_pos > 0 && buf[buf_pos - 1] == '\0')
      --buf_pos;

    // work on buf
    char *next_start = buf;
    do {
      int distance = next_start - buf;
      if (distance > 0) {
        memmove(buf, next_start, sizeof(buf) - distance);
        buf_pos -= distance;
      }

      switch (state) {
      case INIT:
        next_start = strnstr(buf, "='", buf_pos);
        if (next_start) {
          if (strncmp(buf, "StreamTitle", next_start - buf) == 0) {
            state = ARTIST;
          } else {
            state = OTHER;
          }
          next_start += 2;
        }
        break;
      case OTHER:
        next_start = strnstr(buf, ";", buf_pos);
        if (next_start) {
          ++next_start;
          state = INIT;
        } else {
          next_start = buf + buf_pos;
        }
        break;
      case ARTIST:
        next_start = strnstr(buf, " - ", buf_pos);
        if (next_start) {
          *next_start = '\0';
          metadata_cb(STREAM_ARTIST, buf);
          next_start += 3;
          state = TITLE;
        }
        break;
      case TITLE:
        next_start = strnstr(buf, "';", buf_pos);
        if (next_start) {
          *next_start = '\0';
          metadata_cb(STREAM_TITLE, buf);
          next_start += 2;
          state = INIT;
        }
        break;
      }
    } while (next_start && buf_pos > 0);
  }
}

static void stream_task(void *arg) {
  const struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo *res;

  printf("Waiting for DHCP...\n");
  while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  printf("Running DNS lookup for %s\n", stream_host);
  int err = getaddrinfo(stream_host, "80", &hints, &res);
  if (err != 0 || res == NULL) {
    printf("DNS lookup failed err=%d res=%p\n", err, res);
    if (res)
      freeaddrinfo(res);
    goto terminate_task;
  }

  // TODO: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r
  struct in_addr *addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
  printf("DNS lookup succeeded. IP=%s\n", inet_ntoa(*addr));

  int s = socket(res->ai_family, res->ai_socktype, 0);
  if (s < 0) {
    printf("Failed to allocate socket\n");
    freeaddrinfo(res);
    goto terminate_task;
  }

  if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
    freeaddrinfo(res);
    printf("Socket connect failed\n");
    goto close_socket;
  }
  freeaddrinfo(res);

  if (send_http_request(s, stream_host, stream_path) <= 0) {
    printf("Sending HTTP request failed\n");
    goto close_socket;
  }

  int metaint = -1, metapos = -1;
  if (read_header(s, &metaint, &metapos) < 0) {
    printf("Reading reply header failed\n");
    goto close_socket;
  }
  printf("metaint=%d metapos=%d\n", metaint, metapos);

  up_cb();

  int n;
  char buf[64];
  // length of the metadata block in bytes excluding the length field
  int meta_length = 0;
  size_t read_next = sizeof(buf); // TODO
  while (!stop && (n = read(s, buf, read_next)) > 0) {
    if (metaint != -1) {
      metapos += n;
      if (metapos < metaint) {
        fifo_enqueue(buf, n);
        read_next = (size_t)(metaint - metapos);
      } else if (metapos == metaint) {
        fifo_enqueue(buf, n);
        // we have reached the end of the payload,
        // read metadata length field (1 byte) next
        read_next = 1;
      } else {
        if (metapos == metaint + 1) {
          // the first byte after a payload block tells us
          // the length of the following metadata
          meta_length = 16 * (*(const uint8_t *)buf);
        } else {
          parse_metadata(buf, n);
          meta_length -= n;
        }
        if (meta_length == 0) {
          metapos = 0;
          read_next = (size_t)(metaint - metapos);
        } else {
          read_next = (size_t)meta_length;
        }
      }

      if (read_next > sizeof(buf)) {
        read_next = sizeof(buf);
      }
    } else {
      fifo_enqueue(buf, n);
    }
  }

close_socket:
  close(s);
terminate_task:
  vTaskDelete(NULL);
}

int stream_start(const char *host, const char *path, stream_up_cb up,
                 stream_metadata_cb meta) {
  stream_host = host;
  stream_path = path;
  up_cb = up;
  metadata_cb = meta;
  stop = false;

  if (xTaskCreate(stream_task, "stream", 384, NULL, 3, &handle) != pdPASS)
    return 1;

  return 0;
}

void stream_stop(void) {
  stop = true;
  do {
    vTaskDelay(1);
  } while (eTaskGetState(handle) != eDeleted);
}
