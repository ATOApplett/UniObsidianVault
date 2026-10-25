#define _GNU_SOURCE
#include "car.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define QUEUE_LEN    64

#define SERVER_ADDR  "127.0.0.1"
#define SERVER_PORT  3001
#define MAX_MSG_LEN  4096
static volatile sig_atomic_t running = 1;
static const char *car_name;
static const char *floor_low;
static const char *floor_high;
static size_t door_delay_ms;

static void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void wait_until_ms(car_shared_mem *shm, long long when_ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(when_ms / 1000);
    ts.tv_nsec = (long)(when_ms % 1000) * 1000000L;
    pthread_cond_timedwait(&shm->cond, &shm->mutex, &ts);
}

static void str_copy(char *dst, const char *src, size_t cap)
{
    size_t i = 0;
    while (i + 1 < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    while (i < cap)
        dst[i++] = '\0';
}

static bool str_equal(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static void shm_path_for(char *out, size_t cap, const char *name)
{
    snprintf(out, cap, "/cab403_car_%s", name);
}

static void shm_init_sync(car_shared_mem *shm)
{
    pthread_mutexattr_t ma;
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->mutex, &ma);
    pthread_mutexattr_destroy(&ma);

    pthread_condattr_t ca;
    pthread_condattr_init(&ca);
    pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED);
    pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
    pthread_cond_init(&shm->cond, &ca);
    pthread_condattr_destroy(&ca);
}

static car_shared_mem *shm_create(const char *name, const char *lowest_floor)
{
    char path[64];
    shm_path_for(path, sizeof path, name);

    int fd = shm_open(path, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd < 0) {
        fprintf(stderr, "shm_open %s: %s\n", path, strerror(errno));
        return NULL;
    }

    if (ftruncate(fd, sizeof(car_shared_mem)) < 0) {
        fprintf(stderr, "ftruncate %s: %s\n", path, strerror(errno));
        close(fd);
        shm_unlink(path);
        return NULL;
    }

    void *mapping = mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (mapping == MAP_FAILED) {
        fprintf(stderr, "mmap %s: %s\n", path, strerror(errno));
        shm_unlink(path);
        return NULL;
    }

    car_shared_mem *shm = mapping;
    memset(shm, 0, sizeof *shm);
    shm_init_sync(shm);

    str_copy(shm->current_floor, lowest_floor, sizeof shm->current_floor);
    str_copy(shm->target_floor, lowest_floor, sizeof shm->target_floor);
    str_copy(shm->status, "Closed", sizeof shm->status);
    shm->floor_request[0] = '\0';
    return shm;
}

static void shm_destroy(const char *name, car_shared_mem *shm)
{
    char path[64];
    shm_path_for(path, sizeof path, name);
    munmap(shm, sizeof *shm);
    shm_unlink(path);
}

static bool floor_parse(const char *text, int *out)
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
        if (!isdigit((unsigned char)*p) || ++count > max_digits)
            return false;
        n = n * 10 + (*p - '0');
    }

    *out = basement ? -n : n;
    return true;
}

static int floor_number(const char *text)
{
    int n = 0;
    floor_parse(text, &n);
    return n;
}

static bool floor_allowed(const char *text)
{
    int n;
    if (!floor_parse(text, &n))
        return false;
    return n >= floor_number(floor_low) && n <= floor_number(floor_high);
}

static bool floor_next(const char *cur, int dir, char *out)
{
    int n = floor_number(cur) + dir;
    if (n == 0)
        n = dir;
    if (n < floor_number(floor_low) || n > floor_number(floor_high))
        return false;

    char text[16];
    if (n < 0)
        snprintf(text, sizeof text, "B%d", -n);
    else
        snprintf(text, sizeof text, "%d", n);
    str_copy(out, text, 4);
    return true;
}

static bool name_is_valid(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
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

static bool delay_parse(const char *s, size_t *out)
{
    const char *p = s;
    size_t count = 0;
    unsigned long n = 0;

    if (*p < '1' || *p > '9')
        return false;
    for (; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p) || ++count > 5)
            return false;
        n = n * 10 + (unsigned long)(*p - '0');
    }
    if (n > 60000)
        return false;

    *out = (size_t)n;
    return true;
}

static int parse_args(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s {name} {delay} {lowest floor} {highest floor}\n", argv[0]);
        return -1;
    }
    if (!name_is_valid(argv[1])) {
        fprintf(stderr, "Invalid name\n");
        return -1;
    }
    if (!delay_parse(argv[2], &door_delay_ms)) {
        fprintf(stderr, "Invalid delay\n");
        return -1;
    }

    int low, high;
    if (!floor_parse(argv[3], &low) || !floor_parse(argv[4], &high)) {
        fprintf(stderr, "Invalid floor\n");
        return -1;
    }
    if (low >= high) {
        fprintf(stderr, "Lowest floor must be below highest floor\n");
        return -1;
    }

    car_name = argv[1];
    floor_low = argv[3];
    floor_high = argv[4];
    return 0;
}
static int connect_to_server(void)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    if (inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr) > 0 &&
        connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0)
        return fd;

    close(fd);
    return -1;
}

static ssize_t stream_write(int fd, const void *data, size_t len)
{
    const char *p = data;
    size_t left = len;

    while (left > 0) {
        ssize_t w = send(fd, p, left, MSG_NOSIGNAL);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (w == 0)
            return 0;
        p += w;
        left -= (size_t)w;
    }
    return (ssize_t)(len - left);
}

static ssize_t stream_read(int fd, void *data, size_t len)
{
    char *p = data;
    size_t left = len;

    while (left > 0) {
        ssize_t r = recv(fd, p, left, 0);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return 0;
        p += r;
        left -= (size_t)r;
    }
    return (ssize_t)(len - left);
}

static int frame_send(int fd, const char *payload, uint32_t len)
{
    uint32_t header = htonl(len);
    ssize_t r = stream_write(fd, &header, sizeof header);
    if (r <= 0)
        return r == 0 ? 0 : -1;

    r = stream_write(fd, payload, len);
    if (r <= 0)
        return r == 0 ? 0 : -1;
    return 1;
}

static int frame_recv(int fd, char *buf, uint32_t bufsize)
{
    uint32_t header;
    ssize_t r = stream_read(fd, &header, sizeof header);
    if (r <= 0)
        return r == 0 ? 0 : -1;

    uint32_t len = ntohl(header);
    if (len == 0)
        return -1;
    if (len > MAX_MSG_LEN || len >= bufsize) {
        fprintf(stderr, "rejected message: length %u exceeds limit\n", len);
        return -1;
    }

    if (stream_read(fd, buf, len) <= 0)
        return -1;
    buf[len] = '\0';
    return (int)len;
}

typedef struct {
    char cur[4];
    char tgt[4];
    char st[8];
} snapshot_t;

typedef struct {
    car_shared_mem *shm;
    int fd;
    pthread_t rx;
    bool rx_dead;
    snapshot_t sent;
    snapshot_t queue[QUEUE_LEN];
    int qhead, qcount;
    bool has_pending;
    char pending_floor[4];
    long long next_try;
} net_t;

static void snapshot_take(const car_shared_mem *shm, snapshot_t *out)
{
    str_copy(out->cur, shm->current_floor, sizeof out->cur);
    str_copy(out->tgt, shm->target_floor, sizeof out->tgt);
    str_copy(out->st, shm->status, sizeof out->st);
}

static bool snapshot_same(const snapshot_t *a, const snapshot_t *b)
{
    return memcmp(a, b, sizeof *a) == 0;
}

static const snapshot_t *queue_newest(const net_t *n)
{
    if (n->qcount == 0)
        return &n->sent;
    return &n->queue[(n->qhead + n->qcount - 1) % QUEUE_LEN];
}

static void queue_clear(net_t *n)
{
    n->qhead = 0;
    n->qcount = 0;
}

static void net_push(net_t *n)
{
    if (n->fd < 0)
        return;

    snapshot_t now;
    snapshot_take(n->shm, &now);
    if (snapshot_same(&now, queue_newest(n)))
        return;

    if (n->qcount == QUEUE_LEN) {
        n->qhead = (n->qhead + 1) % QUEUE_LEN;
        n->qcount--;
    }
    n->queue[(n->qhead + n->qcount) % QUEUE_LEN] = now;
    n->qcount++;
}

static bool send_text(int fd, const char *text)
{
    return frame_send(fd, text, (uint32_t)strlen(text)) == 1;
}

static bool send_status(int fd, const snapshot_t *s)
{
    char msg[48];
    snprintf(msg, sizeof msg, "STATUS %s %s %s", s->cur, s->tgt, s->st);
    return send_text(fd, msg);
}

static void send_text_unlocked(net_t *n, const char *text)
{
    int fd = n->fd;
    pthread_mutex_unlock(&n->shm->mutex);
    send_text(fd, text);
    pthread_mutex_lock(&n->shm->mutex);
}

static void schedule_retry(net_t *n, long long delay_ms)
{
    n->next_try = now_ms() + delay_ms;
}

static void net_disconnect(net_t *n, long long delay_ms)
{
    if (n->fd < 0)
        return;

    int fd = n->fd;
    pthread_t rx = n->rx;

    shutdown(fd, SHUT_RDWR);
    pthread_mutex_unlock(&n->shm->mutex);
    pthread_join(rx, NULL);
    close(fd);
    pthread_mutex_lock(&n->shm->mutex);

    n->fd = -1;
    n->has_pending = false;
    queue_clear(n);
    schedule_retry(n, delay_ms);
}

static void target_assign(net_t *n, const char *floor_text)
{
    car_shared_mem *shm = n->shm;
    str_copy(shm->target_floor, floor_text, sizeof shm->target_floor);
    if (str_equal(floor_text, shm->current_floor) && str_equal(shm->status, "Closed"))
        str_copy(shm->status, "Opening", sizeof shm->status);
    net_push(n);
}

static void pending_apply(net_t *n)
{
    if (!n->has_pending)
        return;
    target_assign(n, n->pending_floor);
    n->has_pending = false;
    pthread_cond_broadcast(&n->shm->cond);
}

static void floor_command(net_t *n, const char *floor_text)
{
    if (!floor_allowed(floor_text))
        return;

    car_shared_mem *shm = n->shm;
    pthread_mutex_lock(&shm->mutex);
    if (n->fd >= 0 && !shm->service_mode && !shm->emergency_mode) {
        if (str_equal(shm->status, "Between")) {
            str_copy(n->pending_floor, floor_text, sizeof n->pending_floor);
            n->has_pending = true;
        } else {
            target_assign(n, floor_text);
            n->has_pending = false;
        }
        pthread_cond_broadcast(&shm->cond);
    }
    pthread_mutex_unlock(&shm->mutex);
}

static void *receiver_main(void *arg)
{
    net_t *n = arg;
    int fd = n->fd;
    char buf[MAX_MSG_LEN + 1];

    while (frame_recv(fd, buf, sizeof buf) > 0) {
        if (strncmp(buf, "FLOOR ", 6) == 0)
            floor_command(n, buf + 6);
    }

    pthread_mutex_lock(&n->shm->mutex);
    n->rx_dead = true;
    pthread_cond_broadcast(&n->shm->cond);
    pthread_mutex_unlock(&n->shm->mutex);
    return NULL;
}

static void net_setup(net_t *n, car_shared_mem *shm)
{
    memset(n, 0, sizeof *n);
    n->shm = shm;
    n->fd = -1;
    n->next_try = now_ms();
}

static bool net_handle_modes(net_t *n, bool svc, bool emg)
{
    car_shared_mem *shm = n->shm;

    if (svc || emg) {
        if (shm->floor_request_pending) {
            shm->floor_request_pending = 0;
            shm->floor_request[0] = '\0';
            pthread_cond_broadcast(&shm->cond);
        }
        if (n->fd < 0)
            return false;
        send_text_unlocked(n, emg ? "EMERGENCY" : "SERVICE");
        net_disconnect(n, 0);
        return true;
    }
    return false;
}

static void request_clear(car_shared_mem *shm)
{
    shm->floor_request_pending = 0;
    shm->floor_request[0] = '\0';
    pthread_cond_broadcast(&shm->cond);
}

static bool net_forward_request(net_t *n, long long delay_ms, bool svc, bool emg)
{
    car_shared_mem *shm = n->shm;
    if (svc || emg || shm->floor_request_pending != 1)
        return false;

    char req[4];
    memcpy(req, shm->floor_request, sizeof req);
    req[3] = '\0';

    if (!floor_allowed(req)) {
        request_clear(shm);
        return true;
    }
    if (n->fd < 0)
        return false;

    char msg[16];
    snprintf(msg, sizeof msg, "SELECT %s", req);

    int fd = n->fd;
    pthread_mutex_unlock(&shm->mutex);
    bool ok = send_text(fd, msg);
    pthread_mutex_lock(&shm->mutex);

    if (!ok) {
        net_disconnect(n, delay_ms);
        return true;
    }
    if (shm->floor_request_pending == 1 &&
        strncmp(shm->floor_request, req, sizeof req - 1) == 0)
        request_clear(shm);
    return true;
}

static bool net_pump_status(net_t *n, long long delay_ms)
{
    car_shared_mem *shm = n->shm;

    if (n->rx_dead) {
        net_disconnect(n, delay_ms);
        return true;
    }

    snapshot_t out;
    if (n->qcount > 0) {
        out = n->queue[n->qhead];
        n->qhead = (n->qhead + 1) % QUEUE_LEN;
        n->qcount--;
    } else {
        snapshot_take(shm, &out);
        if (snapshot_same(&out, &n->sent))
            return false;
    }

    if (snapshot_same(&out, &n->sent))
        return true;

    int fd = n->fd;
    pthread_mutex_unlock(&shm->mutex);
    bool ok = send_status(fd, &out);
    pthread_mutex_lock(&shm->mutex);

    if (!ok)
        net_disconnect(n, delay_ms);
    else
        n->sent = out;
    return true;
}

static bool net_try_connect(net_t *n, long long now, long long delay_ms, bool svc, bool emg)
{
    car_shared_mem *shm = n->shm;

    if (svc || emg || now < n->next_try)
        return false;

    pthread_mutex_unlock(&shm->mutex);
    int fd = connect_to_server();
    if (fd >= 0) {
        struct timeval tv = {0, 100000};
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    }
    pthread_mutex_lock(&shm->mutex);

    if (fd < 0) {
        schedule_retry(n, delay_ms);
        return true;
    }

    n->fd = fd;
    n->rx_dead = false;
    if (pthread_create(&n->rx, NULL, receiver_main, n) != 0) {
        close(fd);
        n->fd = -1;
        schedule_retry(n, delay_ms);
        return true;
    }

    char hello[96];
    snprintf(hello, sizeof hello, "CAR %s %s %s", car_name, floor_low, floor_high);

    snapshot_t cur;
    snapshot_take(shm, &cur);
    n->sent = cur;
    queue_clear(n);

    pthread_mutex_unlock(&shm->mutex);
    bool ok = send_text(fd, hello) && send_status(fd, &cur);
    pthread_mutex_lock(&shm->mutex);

    if (!ok)
        net_disconnect(n, delay_ms);
    return true;
}

static void *net_main(void *arg)
{
    net_t *n = arg;
    car_shared_mem *shm = n->shm;
    const long long delay_ms = (long long)door_delay_ms;

    pthread_mutex_lock(&shm->mutex);
    while (running) {
        long long now = now_ms();
        bool svc = (shm->service_mode == 1);
        bool emg = (shm->emergency_mode == 1);

        if (net_handle_modes(n, svc, emg))
            continue;
        if (net_forward_request(n, delay_ms, svc, emg))
            continue;

        bool acted = (n->fd >= 0) ? net_pump_status(n, delay_ms) : net_try_connect(n, now, delay_ms, svc, emg);
        if (acted)
            continue;

        if (n->fd < 0 && now < n->next_try)
            wait_until_ms(shm, n->next_try);
        else
            pthread_cond_wait(&shm->cond, &shm->mutex);
    }

    net_disconnect(n, 0);
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);
    return NULL;
}

typedef struct {
    car_shared_mem *shm;
    net_t *net;
    char seen[8];
    long long deadline;
    int move_dir;
    bool arrived;
    bool prev_svc;
    bool prev_emg;
    bool changed;
} car_sm_t;

static void sm_restart_timer(car_sm_t *m, long long now)
{
    m->deadline = now + (long long)door_delay_ms;
}

static void sm_write_status(car_sm_t *m, const char *status, long long now)
{
    str_copy(m->shm->status, status, sizeof m->shm->status);
    str_copy(m->seen, status, sizeof m->seen);
    sm_restart_timer(m, now);
    m->changed = true;
}

static void sm_enter(car_sm_t *m, const char *status, long long now)
{
    sm_write_status(m, status, now);
    net_push(m->net);
}

static void sm_target_to_current(car_sm_t *m)
{
    car_shared_mem *shm = m->shm;
    if (str_equal(shm->target_floor, shm->current_floor))
        return;
    str_copy(shm->target_floor, shm->current_floor, sizeof shm->target_floor);
    m->changed = true;
    net_push(m->net);
}

static bool sm_begin_move(car_sm_t *m, long long now)
{
    car_shared_mem *shm = m->shm;
    int cur, tgt;

    if (!floor_parse(shm->current_floor, &cur))
        return false;
    if (!floor_allowed(shm->target_floor))
        return false;
    floor_parse(shm->target_floor, &tgt);
    if (tgt == cur)
        return false;

    m->move_dir = (tgt > cur) ? 1 : -1;
    sm_enter(m, "Between", now);
    return true;
}

static void sm_end_move(car_sm_t *m, long long now)
{
    car_shared_mem *shm = m->shm;
    char next[4];

    if (floor_next(shm->current_floor, m->move_dir, next))
        str_copy(shm->current_floor, next, sizeof shm->current_floor);
    sm_write_status(m, "Closed", now);

    if (shm->service_mode || shm->emergency_mode) {
        sm_target_to_current(m);
        m->net->has_pending = false;
        m->arrived = false;
        net_push(m->net);
        return;
    }

    pending_apply(m->net);
    m->arrived = str_equal(shm->status, "Closed") &&
                 str_equal(shm->target_floor, shm->current_floor);
}

static void sm_setup(car_sm_t *m, car_shared_mem *shm, net_t *net)
{
    memset(m, 0, sizeof *m);
    m->shm = shm;
    m->net = net;
    str_copy(m->seen, shm->status, sizeof m->seen);
}

static void sm_resync(car_sm_t *m, long long now)
{
    if (str_equal(m->shm->status, m->seen))
        return;
    str_copy(m->seen, m->shm->status, sizeof m->seen);
    sm_restart_timer(m, now);
}

static void sm_watch_modes(car_sm_t *m, bool svc, bool emg)
{
    if (svc && !m->prev_svc) {
        m->net->has_pending = false;
        if (!str_equal(m->shm->status, "Between"))
            sm_target_to_current(m);
    }
    m->prev_svc = svc;

    if (emg && !m->prev_emg)
        m->net->has_pending = false;
    m->prev_emg = emg;
}

static void sm_handle_buttons(car_sm_t *m, long long now)
{
    car_shared_mem *shm = m->shm;
    const char *st = shm->status;

    if (shm->open_button) {
        shm->open_button = 0;
        m->changed = true;
        if (str_equal(st, "Open"))
            sm_restart_timer(m, now);
        else if (str_equal(st, "Closing") || str_equal(st, "Closed"))
            sm_enter(m, "Opening", now);
    }

    if (shm->close_button) {
        shm->close_button = 0;
        m->changed = true;
        if (str_equal(st, "Open"))
            sm_enter(m, "Closing", now);
    }
}

static void sm_handle_timeout(car_sm_t *m, long long now, bool svc)
{
    const char *st = m->shm->status;

    if (str_equal(st, "Closed") || now < m->deadline)
        return;

    if (str_equal(st, "Between")) {
        sm_end_move(m, now);
    } else if (str_equal(st, "Opening")) {
        sm_enter(m, "Open", now);
    } else if (str_equal(st, "Open")) {
        if (svc)
            sm_restart_timer(m, now);
        else
            sm_enter(m, "Closing", now);
    } else if (str_equal(st, "Closing")) {
        sm_enter(m, "Closed", now);
    }
}

static void sm_handle_closed(car_sm_t *m, long long now, bool svc, bool emg)
{
    car_shared_mem *shm = m->shm;

    if (!str_equal(shm->status, "Closed"))
        return;

    if (m->arrived) {
        m->arrived = false;
        if (!svc && !emg)
            sm_enter(m, "Opening", now);
    } else if (svc) {
        if (floor_allowed(shm->target_floor))
            sm_begin_move(m, now);
        else
            sm_target_to_current(m);
    } else if (!emg) {
        sm_begin_move(m, now);
    }
}

static void sm_loop(car_sm_t *m)
{
    car_shared_mem *shm = m->shm;

    while (running) {
        long long now = now_ms();

        sm_resync(m, now);
        bool svc = (shm->service_mode == 1);
        bool emg = (shm->emergency_mode == 1);

        sm_watch_modes(m, svc, emg);
        sm_handle_buttons(m, now);
        sm_handle_timeout(m, now, svc);
        sm_handle_closed(m, now, svc, emg);

        if (m->changed) {
            m->changed = false;
            pthread_cond_broadcast(&shm->cond);
            continue;
        }

        if (str_equal(shm->status, "Closed"))
            pthread_cond_wait(&shm->cond, &shm->mutex);
        else
            wait_until_ms(shm, m->deadline);
    }
}

int main(int argc, char **argv)
{
    if (parse_args(argc, argv) != 0)
        return 1;

    car_shared_mem *shm = shm_create(car_name, floor_low);
    if (shm == NULL)
        return 1;

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    static net_t net;
    net_setup(&net, shm);

    pthread_t net_tid;
    if (pthread_create(&net_tid, NULL, net_main, &net) != 0) {
        shm_destroy(car_name, shm);
        return 1;
    }

    static car_sm_t sm;
    sm_setup(&sm, shm, &net);

    pthread_mutex_lock(&shm->mutex);
    sm_loop(&sm);
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);

    pthread_join(net_tid, NULL);
    shm_destroy(car_name, shm);
    return 0;
}