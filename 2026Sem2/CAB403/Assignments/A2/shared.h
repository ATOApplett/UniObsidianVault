#ifndef SHARED_H
#define SHARED_H

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

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    char cur_flr[4];
    char dstn_flr[4];
    char status[8]; 
    uint8_t open_btn;
    uint8_t close_btn;
    uint8_t door_obst;
    uint8_t overload;
    uint8_t emerg_stop;
    uint8_t indiv_mode;
    uint8_t emerg_mode;
    int delay;
    char high_flr[4];
} car_shared_mem;

void send_looped(int fd, const void *buf, size_t sz);
void recv_looped(int fd, void *buf, size_t sz);

void init_shared_memory(car_shared_mem *shm, const char *low_flr, const char *high_flr, int delay) {
    pthread_mutexattr_t mutattr;
    pthread_mutexattr_init(&mutattr);
    pthread_mutexattr_setpshared(&mutattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->mutex, &mutattr);
    pthread_mutexattr_destroy(&mutattr);

    pthread_condattr_t condattr;
    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shm->cond, &condattr);
    pthread_condattr_destroy(&condattr);

    strcpy(shm->cur_flr, low_flr);
    strcpy(shm->dstn_flr, low_flr);
    strcpy(shm->high_flr, high_flr);
    strcpy(shm->status, "Closed");
    shm->open_btn = 0;
    shm->close_btn = 0;
    shm->door_obst = 0;
    shm->overload = 0;
    shm->emerg_stop = 0;
    shm->indiv_mode = 0;
    shm->emerg_mode = 0;
    shm->delay = delay;
}

void send_message(int fd, const char *buf) {
    uint32_t len = htonl(strlen(buf));
    send_looped(fd, &len, sizeof(len));
    send_looped(fd, buf, strlen(buf));
}

void send_looped(int fd, const void *buf, size_t sz) {
    const char *ptr = buf;
    size_t remain = sz;

    while (remain > 0) {
        ssize_t sent = write(fd, ptr, remain);
        if (sent == -1) {
            perror("write()");
            exit(1);
        }
        ptr += sent;
        remain -= sent;
    }
}
char *receive_msg(int fd) {
    uint32_t nlen;
    recv_looped(fd, &nlen, sizeof(nlen));
    uint32_t len = ntohl(nlen);

    char *buf = malloc(len + 1);
    buf[len] = '\0';
    recv_looped(fd, buf, len);
    return buf;
}

void recv_looped(int fd, void *buf, size_t sz) {
    char *ptr = buf;
    size_t remain = sz;

    while (remain > 0) {
        ssize_t received = read(fd, ptr, remain);
        if (received == -1) {
            perror("read()");
            exit(1);
        }
        if (received == 0) {
            fprintf(stderr, "Connection closed unexpectedly\n");
            exit(1);
        }
        ptr += received;
        remain -= received;
    }
}

#endif // SHARED_H