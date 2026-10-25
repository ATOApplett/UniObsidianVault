#define _GNU_SOURCE
#include "car.h"
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

enum operation {
    OP_NONE,
    OP_OPEN,
    OP_CLOSE,
    OP_STOP,
    OP_SERVICE_ON,
    OP_SERVICE_OFF,
    OP_UP,
    OP_DOWN,
    OP_FLOOR
};

struct operation_entry {
    const char *word;
    enum operation op;
};

static const struct operation_entry OPERATIONS[] = {
    {"open", OP_OPEN},
    {"close", OP_CLOSE},
    {"stop", OP_STOP},
    {"service_on", OP_SERVICE_ON},
    {"service_off", OP_SERVICE_OFF},
    {"up", OP_UP},
    {"down", OP_DOWN},
    {"floor", OP_FLOOR},
};

static enum operation parse_operation(const char *word)
{
    for (size_t i = 0; i < sizeof OPERATIONS / sizeof OPERATIONS[0]; i++) {
        if (strcmp(word, OPERATIONS[i].word) == 0)
            return OPERATIONS[i].op;
    }
    return OP_NONE;
}

static void show_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s {open|close|stop|service_on|service_off|up|down} {car name} %s floor {car name} {floor}\n"
            prog, prog);
}

static car_shared_mem *attach_car(const char *name)
{
    char path[64];
    snprintf(path, sizeof path, "/cab403_car_%s", name);

    car_shared_mem *mapped = NULL;
    struct stat info;
    int fd = shm_open(path, O_RDWR, 0);

    if (fd >= 0 && fstat(fd, &info) == 0 &&
        (size_t)info.st_size >= sizeof(car_shared_mem)) {
        void *region = mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (region != MAP_FAILED)
            mapped = region;
    }

    if (fd >= 0)
        close(fd);
    if (mapped == NULL)
        fprintf(stderr, "Unable to access car %s.\n", name);
    return mapped;
}

static bool status_equals(const car_shared_mem *car, const char *text)
{
    if (memchr(car->status, '\0', sizeof car->status) == NULL)
        return false;
    return strcmp(car->status, text) == 0;
}

static bool name_ok(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    size_t count = 0;

    if (!isalnum(*p))
        return false;
    for (; *p != '\0'; p++) {
        if (++count > 31)
            return false;
        if (!isalnum(*p) && *p != '_' && *p != '-')
            return false;
    }
    return true;
}

static bool floor_ok(const char *text, int *out)
{
    const char *p = text;
    bool basement = (*p == 'B');
    size_t max_digits = basement ? 2 : 3;
    size_t count = 0;
    int n = 0;

    if (basement)
        p++;
    if (*p < '1' || *p > '9')
        return false;

    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9' || ++count > max_digits)
            return false;
        n = n * 10 + (*p - '0');
    }

    *out = basement ? -n : n;
    return true;
}

static int reject(const char *text)
{
    fputs(text, stderr);
    return 1;
}

static int do_service_on(car_shared_mem *car)
{
    car->service_mode = 1;
    car->emergency_mode = 0;
    car->floor_request_pending = 0;
    car->floor_request[0] = '\0';
    return 0;
}

static int do_floor_request(car_shared_mem *car, const char *floor_text)
{
    if (car->service_mode == 1)
        return reject("Operation not allowed while in service mode.\n");
    if (car->emergency_mode == 1)
        return reject("Operation not allowed while in emergency mode.\n");
    if (car->floor_request_pending == 1)
        return reject("A floor request is already pending.\n");

    size_t len = strnlen(floor_text, sizeof car->floor_request - 1);
    memset(car->floor_request, 0, sizeof car->floor_request);
    memcpy(car->floor_request, floor_text, len);
    car->floor_request_pending = 1;
    return 0;
}

static int do_service_step(car_shared_mem *car, const char *name, int dir)
{
    int current;

    if (car->service_mode != 1)
        return reject("Operation only allowed in service mode.\n");
    if (status_equals(car, "Between"))
        return reject("Operation not allowed while elevator is moving.\n");
    if (!status_equals(car, "Closed"))
        return reject("Operation not allowed while doors are open.\n");
    if (strcmp(car->target_floor, car->current_floor) != 0)
        return reject("Operation not allowed while elevator is moving.\n");
    if (!floor_ok(car->current_floor, &current)) {
        fprintf(stderr, "Unable to access car %s.\n", name);
        return 1;
    }

    int next = current + dir;
    if (next == 0)
        next = dir;
    if (next > 999 || next < -99)
        return reject("Invalid floor specified.\n");

    if (next < 0)
        snprintf(car->target_floor, sizeof car->target_floor, "B%d", -next);
    else
        snprintf(car->target_floor, sizeof car->target_floor, "%d", next);
    return 0;
}

static int run_operation(car_shared_mem *car, enum operation op, const char *name, const char *floor_text)
{
    switch (op) {
    case OP_OPEN:
        car->open_button = 1;
        return 0;
    case OP_CLOSE:
        car->close_button = 1;
        return 0;
    case OP_STOP:
        car->emergency_stop = 1;
        return 0;
    case OP_SERVICE_ON:
        return do_service_on(car);
    case OP_SERVICE_OFF:
        car->service_mode = 0;
        return 0;
    case OP_FLOOR:
        return do_floor_request(car, floor_text);
    case OP_UP:
        return do_service_step(car, name, 1);
    case OP_DOWN:
        return do_service_step(car, name, -1);
    case OP_NONE:
        break;
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        show_usage(argv[0]);
        return 1;
    }

    enum operation op = parse_operation(argv[1]);
    const char *name = argv[2];

    if (op == OP_NONE) {
        fprintf(stderr, "Invalid operation.\n");
        show_usage(argv[0]);
        return 1;
    }
    if (argc != (op == OP_FLOOR ? 4 : 3)) {
        show_usage(argv[0]);
        return 1;
    }
    if (!name_ok(name))
        return reject("Invalid car name.\n");

    const char *floor_text = NULL;
    if (op == OP_FLOOR) {
        int parsed = 0;
        floor_text = argv[3];
        if (!floor_ok(floor_text, &parsed))
            return reject("Invalid floor specified.\n");
    }

    car_shared_mem *car = attach_car(name);
    if (car == NULL)
        return 1;

    pthread_mutex_lock(&car->mutex);
    int status = run_operation(car, op, name, floor_text);
    if (status == 0)
        pthread_cond_broadcast(&car->cond);
    pthread_mutex_unlock(&car->mutex);

    munmap(car, sizeof *car);
    return status;
}