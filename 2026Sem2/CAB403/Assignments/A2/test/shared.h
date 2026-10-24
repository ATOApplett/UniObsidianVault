#ifndef CAB403_TEST_SHARED_H
#define CAB403_TEST_SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/wait.h>

#define CONTROLLER_PORT 3001
#define SHARED_MEMORY_PREFIX "/cab403_car_"
#define SAFETY_HEARTBEAT_INTERVAL_MS 100U
#define SAFETY_HEARTBEAT_TIMEOUT_MS 500U

typedef struct
{
  pthread_mutex_t mutex;           // Locked while the contents of the structure are being accessed/modified
  pthread_cond_t cond;             // Signalled when the contents of the structure change
  char status[8];                  // NUL-terminated C string indicating the elevator's status
  char current_floor[4];           // NUL-terminated C string in the range of "B99" to "B1" and "1" to "999"
  char target_floor[4];            // Controller's immediate movement target
  char floor_request[4];           // Passenger floor selection mailbox
  uint8_t open_button;             // 1 if open doors button is pressed, 0 otherwise
  uint8_t close_button;            // 1 if close doors button is pressed, 0 otherwise
  uint8_t floor_request_pending;   // 1 while floor_request awaits transmission
  uint8_t door_obstruction;        // 1 if obstruction detected, 0 otherwise
  uint8_t overload;                // 1 if overload detected
  uint8_t emergency_stop;          // 1 if emergency stop button has been pressed, 0 otherwise
  uint8_t service_mode;            // 1 while in individual service mode
  uint8_t emergency_mode;          // 0 if not in emergency mode, 1 if in emergency mode
  uint32_t safety_heartbeat;       // Sequence incremented by the safety process
} car_shared_mem;

void recv_looped(int fd, void *buf, size_t sz)
{
  char *ptr = buf;
  size_t remain = sz;

  while (remain > 0) {
    ssize_t received = read(fd, ptr, remain);
    if (received == 0) {
      fprintf(stderr, "Unexpected end of stream.\n");
      exit(1);
    }
    if (received == -1) {
      if (errno == EINTR) continue;
      perror("read()");
      exit(1);
    }
    ptr += received;
    remain -= received;
  }
}

char *receive_msg(int fd)
{
  uint32_t nlen;
  recv_looped(fd, &nlen, sizeof(nlen));
  uint32_t len = ntohl(nlen);

  if (len > 4096U) {
    fprintf(stderr, "Protocol message is too long.\n");
    exit(1);
  }

  char *buf = malloc((size_t)len + 1U);
  if (buf == NULL) {
    perror("malloc()");
    exit(1);
  }
  buf[len] = '\0';
  recv_looped(fd, buf, len);
  return buf;
}

void send_looped(int fd, const void *buf, size_t sz)
{
  const char *ptr = buf;
  size_t remain = sz;

  while (remain > 0) {
    ssize_t sent = write(fd, ptr, remain);
    if (sent == -1) {
      if (errno == EINTR) continue;
      perror("write()");
      exit(1);
    }
    ptr += sent;
    remain -= sent;
  }
}

void send_message(int fd, const char *buf)
{
  uint32_t len = htonl((uint32_t)strlen(buf));
  send_looped(fd, &len, sizeof(len));
  send_looped(fd, buf, strlen(buf));
}

void msg(const char *string)
{
  printf("### %s\n    ", string);
  fflush(stdout);
}

void reset_shm(car_shared_mem *s)
{
  pthread_mutex_lock(&s->mutex);
  size_t offset = offsetof(car_shared_mem, status);
  memset((char *)s + offset, 0, sizeof(*s) - offset);

  strcpy(s->status, "Closed");
  strcpy(s->current_floor, "1");
  strcpy(s->target_floor, "1");
  pthread_mutex_unlock(&s->mutex);
}

void init_shm(car_shared_mem *s)
{
  pthread_mutexattr_t mutattr;
  pthread_mutexattr_init(&mutattr);
  pthread_mutexattr_setpshared(&mutattr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&s->mutex, &mutattr);
  pthread_mutexattr_destroy(&mutattr);

  pthread_condattr_t condattr;
  pthread_condattr_init(&condattr);
  pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
  pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
  pthread_cond_init(&s->cond, &condattr);
  pthread_condattr_destroy(&condattr);

  reset_shm(s);
}

#endif
