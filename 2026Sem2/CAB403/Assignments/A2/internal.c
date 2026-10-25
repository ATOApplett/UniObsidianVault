#include "shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

#define SHM_NAME_PREFIX "/car"

void update_flr(car_shared_mem *shared_mem, const char *new_flr) {
    strncpy(shared_mem->dstn_flr, new_flr, sizeof(shared_mem->dstn_flr) - 1);
    shared_mem->dstn_flr[sizeof(shared_mem->dstn_flr) - 1] = '\0';
}

int is_between_flrs(car_shared_mem *shared_mem) {
    return strcmp(shared_mem->status, "Between") == 0;
}

int are_doors_closed(car_shared_mem *shared_mem) {
    return strcmp(shared_mem->status, "Closed") == 0;
}
int are_doors_open(car_shared_mem *shared_mem) {
    return strcmp(shared_mem->status, "Open") == 0 || strcmp(shared_mem->status, "Opening") == 0;
}

int get_flr_val(const char *flr) {
    if (flr[0] == 'B') {
        return -atoi(flr + 1);
    }
    return atoi(flr);
}
void set_flr_str(char *flr_str, int flr_val) {
    if (flr_val < 0) {
        snprintf(flr_str, 4, "B%d", -flr_val);
    } else {
        snprintf(flr_str, 4, "%d", flr_val);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Invalid arguments.\n");
        return 1;
    }

    char *car_name = argv[1];
    char *operation = argv[2];
    char shm_name[32];
    snprintf(shm_name, sizeof(shm_name), "%s%s", SHM_NAME_PREFIX, car_name);

    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        printf("Unable to access car %s.\n", car_name);
        return 1;
    }

    car_shared_mem *shared_mem = mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        close(shm_fd);
        printf("Failed to map shared memory.\n");
        return 1;
    }

    if (pthread_mutex_lock(&shared_mem->mutex) != 0) {
        munmap(shared_mem, sizeof(car_shared_mem));
        close(shm_fd);
        printf("Failed to lock mutex.\n");
        return 1;
    }

    int success = 0;

    shared_mem->open_btn = 0;
    shared_mem->close_btn = 0;

    if (strcmp(operation, "open") == 0) {
        if (are_doors_closed(shared_mem)) {
            shared_mem->open_btn = 1;
            strcpy(shared_mem->status, "Opening");
            success = 1;
        } else {
            printf("Operation not allowed while doors are open.\n");
        }
    } else if (strcmp(operation, "close") == 0) {
        if (are_doors_open(shared_mem)) {
            shared_mem->close_btn = 1;
            strcpy(shared_mem->status, "Closed");
            success = 1;
        } else if (are_doors_closed(shared_mem)) {
            printf("Doors are already closed.\n");
        } else {
            printf("Operation not allowed while doors are closed.\n");
        }
    } else if (strcmp(operation, "stop") == 0) {
        shared_mem->emerg_stop = 1;
        shared_mem->emerg_mode = 1;
        success = 1;
    } else if (strcmp(operation, "service_on") == 0) {
        shared_mem->indiv_mode = 1;
        shared_mem->emerg_mode = 0;
        success = 1;
    } else if (strcmp(operation, "service_off") == 0) {
        shared_mem->indiv_mode = 0;
        success = 1;
    } else if (strcmp(operation, "up") == 0) {
        if (!shared_mem->indiv_mode) {
            printf("Operation only allowed in service mode.\n");
        } else if (!are_doors_closed(shared_mem)) {
            printf("Operation not allowed while doors are open.\n");
        } else if (is_between_flrs(shared_mem)) {
            printf("Operation not allowed while elevator is moving.\n");
        } else {
            int cur_flr = get_flr_val(shared_mem->cur_flr);
            if (cur_flr < 999) {
                char new_flr[4];
                set_flr_str(new_flr, cur_flr + 1);
                update_flr(shared_mem, new_flr);
                success = 1;
            } else {
                printf("Already at the highest floor.\n");
            }
        }
    } else if (strcmp(operation, "down") == 0) {
        if (!shared_mem->indiv_mode) {
            printf("Operation only allowed in service mode.\n");
        } else if (!are_doors_closed(shared_mem)) {
            printf("Operation not allowed while doors are open.\n");
        } else if (is_between_flrs(shared_mem)) {
            printf("Operation not allowed while elevator is moving.\n");
        } else {
            int cur_flr = get_flr_val(shared_mem->cur_flr);
            if (cur_flr > -99) { 
                char new_flr[4];
                set_flr_str(new_flr, cur_flr - 1);
                update_flr(shared_mem, new_flr);
                success = 1;
            } else {
                printf("Already at the lowest floor.\n");
            }
        }
    } else {
        printf("Invalid operation.\n");
    }

    if (success) {
        pthread_cond_broadcast(&shared_mem->cond);
    }

    pthread_mutex_unlock(&shared_mem->mutex);

    munmap(shared_mem, sizeof(car_shared_mem));
    close(shm_fd);

    return 0;
}