#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define MILLISECOND 1000 
#define CONTROLLER_IP "127.0.0.1"
#define CONTROLLER_PORT 3000

#include "shared.h"


// Function prototypes
void handle_door_timing(car_shared_mem *shm);
void handle_indiv_mode(car_shared_mem *shm);
void handle_closing_timing(car_shared_mem *shm);
void move_one_flr(car_shared_mem *shm);
void cleanup_resources();
void signal_handler(int sig);
static int flr_to_int(const char *label) {
    return label[0] == 'B' ? -atoi(label + 1) : atoi(label);
}

// Global variables
int shm_fd;
static car_shared_mem *shm;
char shm_name[64];

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s {name} {lowest floor} {highest floor} {delay}\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *name = argv[1];
    const char *low_flr = argv[2];
    const char *high_flr = argv[3];
    int delay = atoi(argv[4]);
    snprintf(shm_name, sizeof(shm_name), "/car%s", name);
    signal(SIGINT, signal_handler);
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to create/open shared memory");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, sizeof(car_shared_mem)) == -1) {
        perror("Failed to set shared memory size");
        close(shm_fd);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
    }
    shm = mmap(0, sizeof(car_shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("Failed to map shared memory");
        close(shm_fd);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
    }
    init_shared_memory(shm, low_flr, high_flr, delay);
    signal(SIGPIPE, SIG_IGN);
    while (1) {
        pthread_mutex_lock(&shm->mutex);
        if (shm->open_btn == 1) {
            shm->open_btn = 0;
            pthread_cond_broadcast(&shm->cond);
            pthread_mutex_unlock(&shm->mutex);
            handle_door_timing(shm);
            continue;
        }
        if (shm->indiv_mode == 1) {
            pthread_mutex_unlock(&shm->mutex);
            handle_indiv_mode(shm);
            continue;
        }
        pthread_mutex_unlock(&shm->mutex);
        usleep(5 * MILLISECOND);
    }
    cleanup_resources();
    return 0;
}

void handle_door_timing(car_shared_mem *shm) {
    int open_time = shm->delay * MILLISECOND;
    pthread_mutex_lock(&shm->mutex);
    strcpy(shm->status, "Opening");
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);
    for (int elapsed = 0; elapsed < open_time; elapsed += 5 * MILLISECOND) {
        usleep(5 * MILLISECOND);
        pthread_mutex_lock(&shm->mutex);
        if (shm->close_btn == 1) {
            shm->close_btn = 0;
            strcpy(shm->status, "Closing");
            pthread_cond_broadcast(&shm->cond);
            pthread_mutex_unlock(&shm->mutex);
            handle_closing_timing(shm);
            return;
        }
        pthread_mutex_unlock(&shm->mutex);
    }
    pthread_mutex_lock(&shm->mutex);
    strcpy(shm->status, "Open");
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);
    for (int elapsed = 0; elapsed < open_time; elapsed += 5 * MILLISECOND) {
        usleep(5 * MILLISECOND);
        pthread_mutex_lock(&shm->mutex);
        if (shm->close_btn == 1) {
            shm->close_btn = 0;
            strcpy(shm->status, "Closing");
            pthread_cond_broadcast(&shm->cond);
            pthread_mutex_unlock(&shm->mutex);
            handle_closing_timing(shm);
            return;
        }
        pthread_mutex_unlock(&shm->mutex);
    }
    pthread_mutex_lock(&shm->mutex);
    strcpy(shm->status, "Closing");
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);
    for (int elapsed = 0; elapsed < open_time; elapsed += 5 * MILLISECOND) {
        usleep(5 * MILLISECOND);
        pthread_mutex_lock(&shm->mutex);
        if (shm->close_btn == 1) {
            shm->close_btn = 0;
            strcpy(shm->status, "Closing");
            pthread_cond_broadcast(&shm->cond);
            pthread_mutex_unlock(&shm->mutex);
            handle_closing_timing(shm);
            return;
        }
        pthread_mutex_unlock(&shm->mutex);
    }
    pthread_mutex_lock(&shm->mutex);
    strcpy(shm->status, "Closed");
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);
}

void handle_closing_timing(car_shared_mem *shm) {
    int close_time = shm->delay * MILLISECOND;
    pthread_mutex_lock(&shm->mutex);
    strcpy(shm->status, "Closing");
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);
    usleep(close_time);
    pthread_mutex_lock(&shm->mutex);
    strcpy(shm->status, "Closed");
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);
}

void handle_indiv_mode(car_shared_mem *shm) {
    while (1) {
        pthread_mutex_lock(&shm->mutex);
        if (shm->indiv_mode != 1) {
            pthread_mutex_unlock(&shm->mutex);
            return;
        }

        if (strcmp(shm->status, "Closed") == 0) {
            int dest_flr = flr_to_int(shm->dstn_flr);
            int cur_flr = flr_to_int(shm->cur_flr);
            int high_flr = flr_to_int(shm->high_flr);

            if (dest_flr > high_flr) {
                strcpy(shm->dstn_flr, shm->cur_flr);
            }
            else if (dest_flr < cur_flr) {
                pthread_mutex_unlock(&shm->mutex);
                move_one_flr(shm);
                return;
            }
            else if (dest_flr > cur_flr) {
                pthread_mutex_unlock(&shm->mutex);
                move_one_flr(shm);
                return;
            }
        }
        if (shm->open_btn == 1) {
            shm->open_btn = 0;
            pthread_cond_broadcast(&shm->cond);
            pthread_mutex_unlock(&shm->mutex);
            handle_door_timing(shm);
            continue;
        }

        pthread_mutex_unlock(&shm->mutex);
        usleep(5 * MILLISECOND);
    }
}

void move_one_flr(car_shared_mem *shm) {
    int delay_time = shm->delay * MILLISECOND;

    pthread_mutex_lock(&shm->mutex);
    strcpy(shm->status, "Between");
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);

    usleep(delay_time);

    pthread_mutex_lock(&shm->mutex);
    strcpy(shm->cur_flr, shm->dstn_flr);
    strcpy(shm->status, "Closed");
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);
}

void signal_handler(int sig) {
    if (sig == SIGINT) {
        cleanup_resources();
        exit(0);
    }
}

void cleanup_resources() {
    if (shm != MAP_FAILED) {
        munmap(shm, sizeof(car_shared_mem));
    }
    if (shm_fd != -1) {
        close(shm_fd);
    }
    shm_unlink(shm_name);
}

int connect_to_controller(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(3000);
  sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(fd, (const struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1)
  {
    perror("connect()");
    exit(1);
  }
  return fd;
}