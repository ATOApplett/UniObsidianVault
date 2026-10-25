#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "car.h"

#define CAR_NAME_MAX     31U
#define PATH_CAP         64U
#define TEXT_CAP         128U

#define FLAG_STOP        0x1U
#define FLAG_OVERLOAD    0x2U
#define FLAG_DATA        0x4U

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

static size_t bounded_len(const char *text, size_t cap)
{
    const char *p = text;
    const char *end = text + cap;

    while (p < end && *p != '\0')
        p++;
    return (size_t)(p - text);
}

static void emit(const char *text)
{
    ssize_t written = write(STDERR_FILENO, text, bounded_len(text, TEXT_CAP));
    (void)written;
}

static bool field_equals(const char *field, size_t cap, const char *text)
{
    for (size_t i = 0; i < cap; i++) {
        if (field[i] != text[i])
            return false;
        if (field[i] == '\0')
            return true;
    }
    return false;
}

static bool floor_field_ok(const char *field, size_t cap)
{
    size_t len = bounded_len(field, cap);
    size_t skip = field[0] == 'B' ? 1U : 0U;
    size_t max_digits = skip == 1U ? 2U : 3U;

    if (len >= cap || len <= skip)
        return false;
    if (len - skip > max_digits || field[skip] == '0')
        return false;

    for (size_t i = skip; i < len; i++) {
        if (field[i] < '0' || field[i] > '9')
            return false;
    }
    return true;
}

static bool state_equals(const car_shared_mem *car, const char *text)
{
    return field_equals(car->status, sizeof car->status, text);
}

static bool any_flag_out_of_range(const car_shared_mem *car)
{
    const unsigned int values[] = {
        car->open_button,
        car->close_button,
        car->floor_request_pending,
        car->door_obstruction,
        car->overload,
        car->emergency_stop,
        car->service_mode,
        car->emergency_mode,
    };

    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        if (values[i] > 1U)
            return true;
    }
    return false;
}

static bool state_name_unknown(const car_shared_mem *car)
{
    static const char *const known[] = {"Opening", "Closing", "Open", "Closed", "Between"};

    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++) {
        if (state_equals(car, known[i]))
            return false;
    }
    return true;
}

static bool data_is_broken(const car_shared_mem *car)
{
    if (!floor_field_ok(car->current_floor, sizeof car->current_floor))
        return true;
    if (!floor_field_ok(car->target_floor, sizeof car->target_floor))
        return true;
    if (car->floor_request_pending == 1U &&
        !floor_field_ok(car->floor_request, sizeof car->floor_request))
        return true;
    if (state_name_unknown(car))
        return true;
    if (any_flag_out_of_range(car))
        return true;
    if (car->door_obstruction == 1U && !state_equals(car, "Opening") &&
        !state_equals(car, "Closing"))
        return true;
    return false;
}

static uint32_t scan_hazards(car_shared_mem *car, bool *dirty)
{
    uint32_t flags = 0U;

    if (car->door_obstruction == 1U && state_equals(car, "Closing")) {
        memcpy(car->status, "Opening", sizeof "Opening");
        *dirty = true;
    }

    if (car->emergency_stop == 1U && car->emergency_mode == 0U) {
        flags |= FLAG_STOP;
        car->emergency_mode = 1U;
        car->emergency_stop = 0U;
        *dirty = true;
    }

    if (car->overload == 1U && car->emergency_mode == 0U) {
        flags |= FLAG_OVERLOAD;
        car->emergency_mode = 1U;
        *dirty = true;
    }

    if (car->emergency_mode == 0U && data_is_broken(car)) {
        flags |= FLAG_DATA;
        car->emergency_mode = 1U;
        *dirty = true;
    }

    return flags;
}

static void announce(uint32_t flags)
{
    if (flags & FLAG_STOP)
        emit("The emergency button has been pressed! Entering emergency mode.\n");
    if (flags & FLAG_OVERLOAD)
        emit("Elevator car is overloaded! Entering emergency mode.\n");
    if (flags & FLAG_DATA)
        emit("Data consistency error!\n");
}

static bool car_path(char *out, const char *name)
{
    static const char prefix[] = "/cab403_car_";
    const size_t prefix_len = sizeof prefix - 1U;
    const size_t len = bounded_len(name, PATH_CAP);

    if (len < 1U || len > CAR_NAME_MAX)
        return false;
    if (memchr(name, '/', len) != NULL)
        return false;

    memcpy(out, prefix, prefix_len);
    memcpy(out + prefix_len, name, len);
    out[prefix_len + len] = '\0';
    return true;
}

static car_shared_mem *map_car(const char *path)
{
    int fd = shm_open(path, O_RDWR, 0);
    if (fd < 0)
        return NULL;

    car_shared_mem *mapped = NULL;
    struct stat info;
    memset(&info, 0, sizeof info);

    if (fstat(fd, &info) == 0 && info.st_size >= 0 &&
        (uint64_t)info.st_size >= (uint64_t)sizeof(car_shared_mem)) {
        void *region = mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, 0);
        if (region != MAP_FAILED)
            mapped = region;
    }

    close(fd);
    return mapped;
}

static bool monitor_step(car_shared_mem *car, bool *holding)
{
    bool dirty = false;
    uint32_t flags = scan_hazards(car, &dirty);

    if (dirty && pthread_cond_broadcast(&car->cond) != 0)
        return false;

    if (flags == 0U)
        return pthread_cond_wait(&car->cond, &car->mutex) == 0;

    if (pthread_mutex_unlock(&car->mutex) != 0)
        return false;
    *holding = false;

    announce(flags);

    bool relocked = pthread_mutex_lock(&car->mutex) == 0;
    *holding = relocked;
    return relocked;
}

static int monitor(car_shared_mem *car)
{
    if (pthread_mutex_lock(&car->mutex) != 0)
        return EXIT_FAILURE;

    bool holding = true;
    bool healthy = true;

    while (running != 0 && healthy)
        healthy = monitor_step(car, &holding);

    if (holding)
        healthy = healthy && pthread_mutex_unlock(&car->mutex) == 0;

    return healthy ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool set_up_signals(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof action);
    action.sa_handler = handle_sigint;
    if (sigemptyset(&action.sa_mask) != 0)
        return false;
    return sigaction(SIGINT, &action, NULL) == 0;
}

static int run(const char *car_name)
{
    char path[PATH_CAP] = {0};
    car_shared_mem *car = NULL;

    if (car_path(path, car_name))
        car = map_car(path);

    if (car == NULL) {
        emit("Unable to access car ");
        emit(car_name);
        emit(".\n");
        return EXIT_FAILURE;
    }

    int result = EXIT_FAILURE;
    if (set_up_signals())
        result = monitor(car);
    else
        emit("Unable to install signal handler.\n");

    if (munmap(car, sizeof *car) != 0)
        result = EXIT_FAILURE;
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        emit("Usage: safety {car name}\n");
        return EXIT_FAILURE;
    }
    return run(argv[1]);
}