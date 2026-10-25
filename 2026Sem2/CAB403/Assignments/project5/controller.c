#define _GNU_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>

#define BIND_ADDR    "127.0.0.1"
#define BIND_PORT    3001
#define MAX_MSG_LEN  4096

#define CAR_SLOTS    32
#define CONN_SLOTS   256
#define SWEEP_SLOTS  16
#define STOP_SLOTS   1100
#define NO_FLOOR     INT_MIN

static int listen_socket(void)
{
    struct sockaddr_in addr;
    int one = 1;
    const char *failed = NULL;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BIND_PORT);

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0)
        failed = "setsockopt";
    else if (inet_pton(AF_INET, BIND_ADDR, &addr.sin_addr) <= 0)
        failed = "inet_pton";
    else if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0)
        failed = "bind";
    else if (listen(fd, SOMAXCONN) < 0)
        failed = "listen";

    if (failed != NULL) {
        perror(failed);
        close(fd);
        return -1;
    }
    return fd;
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
        if (*p < '0' || *p > '9' || ++count > max_digits)
            return false;
        n = n * 10 + (*p - '0');
    }

    *out = basement ? -n : n;
    return true;
}

static bool floor_ordinal(const char *text, int *ordinal)
{
    int value;
    if (!floor_parse(text, &value))
        return false;
    *ordinal = value < 0 ? value + 1 : value;
    return true;
}

static void ordinal_text(int ordinal, char out[16])
{
    if (ordinal > 0)
        snprintf(out, 16, "%d", ordinal);
    else
        snprintf(out, 16, "B%d", 1 - ordinal);
}

typedef struct {
    int dir;
    int count;
    int stops[STOP_SLOTS];
} route_t;

typedef struct {
    bool active;
    uint64_t serial;
    int fd;
    char name[32];
    int low, high;
    bool reported;
    int here, goal;
    char state[8];
    int sent_goal;
    int route_count;
    route_t routes[SWEEP_SLOTS];
} lift_t;

static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;
static lift_t lifts[CAR_SLOTS];
static uint64_t serial_counter = 1;
static int conn_table[CONN_SLOTS];

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

static bool send_frame(int fd, const char *msg)
{
    size_t len = strlen(msg);
    if (len > 128)
        return false;

    uint8_t packet[4 + 128];
    uint32_t prefix = htonl((uint32_t)len);
    memcpy(packet, &prefix, sizeof prefix);
    memcpy(packet + sizeof prefix, msg, len);

    size_t total = sizeof prefix + len;
    ssize_t r;
    do {
        r = send(fd, packet, total, MSG_NOSIGNAL | MSG_DONTWAIT);
    } while (r < 0 && errno == EINTR);

    return r == (ssize_t)total;
}

static void drop_lift(lift_t *l)
{
    if (!l->active)
        return;
    shutdown(l->fd, SHUT_RDWR);
    l->active = false;
    l->route_count = 0;
}

static int sign_of(int x)
{
    if (x > 0)
        return 1;
    return x < 0 ? -1 : 0;
}

static int lift_position(const lift_t *l)
{
    bool travelling = strcmp(l->state, "Closing") == 0 || strcmp(l->state, "Between") == 0;
    if (!travelling)
        return l->here;

    int dir = sign_of(l->goal - l->here);
    if (dir == 0 && l->route_count > 0)
        dir = sign_of(l->routes[0].stops[0] - l->here);
    if (dir == 0 && l->route_count > 0)
        dir = l->routes[0].dir;
    if (dir == 0)
        return l->here;

    int p = l->here + dir;
    if (p < l->low)
        p = l->low;
    if (p > l->high)
        p = l->high;
    return p;
}

static bool route_add_stop(route_t *r, int f)
{
    int i;
    for (i = 0; i < r->count; i++) {
        if (r->dir * r->stops[i] >= r->dir * f)
            break;
    }
    if (i < r->count && r->stops[i] == f)
        return true;
    if (r->count >= STOP_SLOTS)
        return false;

    memmove(&r->stops[i + 1], &r->stops[i], (size_t)(r->count - i) * sizeof(int));
    r->stops[i] = f;
    r->count++;
    return true;
}

static bool plan_stop(lift_t *l, int f, int want_dir)
{
    int pos = lift_position(l);

    for (int k = 0; k < l->route_count; k++) {
        route_t *r = &l->routes[k];
        bool wrong_dir = want_dir != 0 && r->dir != want_dir;
        bool behind = k == 0 && r->dir * (f - pos) < 0;
        if (wrong_dir || behind)
            continue;
        return route_add_stop(r, f);
    }

    if (l->route_count >= SWEEP_SLOTS)
        return false;

    int dir;
    if (want_dir != 0)
        dir = want_dir;
    else if (l->route_count > 0)
        dir = -l->routes[l->route_count - 1].dir;
    else
        dir = f >= pos ? 1 : -1;

    route_t *fresh = &l->routes[l->route_count++];
    fresh->dir = dir;
    fresh->count = 0;
    return route_add_stop(fresh, f);
}

static void pop_front(lift_t *l)
{
    route_t *r = &l->routes[0];
    memmove(&r->stops[0], &r->stops[1], (size_t)(r->count - 1) * sizeof(int));
    r->count--;
    if (r->count == 0) {
        memmove(&l->routes[0], &l->routes[1], (size_t)(l->route_count - 1) * sizeof(route_t));
        l->route_count--;
    }
}

static void push_command(lift_t *l)
{
    if (!l->active || !l->reported)
        return;
    if (l->route_count == 0) {
        l->sent_goal = NO_FLOOR;
        return;
    }

    int first = l->routes[0].stops[0];
    if (first == l->sent_goal)
        return;

    char text[16];
    char msg[32];
    ordinal_text(first, text);
    snprintf(msg, sizeof msg, "FLOOR %s", text);
    if (!send_frame(l->fd, msg)) {
        drop_lift(l);
        return;
    }
    l->sent_goal = first;
}

static lift_t *enroll_lift(int fd, const char *name, int low, int high)
{
    for (int i = 0; i < CAR_SLOTS; i++) {
        if (lifts[i].active && strcmp(lifts[i].name, name) == 0)
            drop_lift(&lifts[i]);
    }

    int slot = -1;
    for (int i = 0; i < CAR_SLOTS && slot < 0; i++) {
        if (!lifts[i].active)
            slot = i;
    }
    if (slot < 0)
        return NULL;

    lift_t *l = &lifts[slot];
    l->active = true;
    l->serial = serial_counter++;
    l->fd = fd;
    snprintf(l->name, sizeof l->name, "%s", name);
    l->low = low;
    l->high = high;
    l->reported = false;
    l->here = low;
    l->goal = low;
    strcpy(l->state, "Closed");
    l->sent_goal = NO_FLOOR;
    l->route_count = 0;
    return l;
}

static bool state_is_valid(const char *state)
{
    static const char *const valid[] = {"Opening", "Open", "Closing", "Closed", "Between"};
    for (size_t i = 0; i < sizeof valid / sizeof valid[0]; i++) {
        if (strcmp(state, valid[i]) == 0)
            return true;
    }
    return false;
}

static bool apply_status(lift_t *l, const char *here_text, const char *goal_text, const char *state)
{
    int here, goal;
    if (!floor_ordinal(here_text, &here) || !floor_ordinal(goal_text, &goal))
        return false;
    if (here < l->low || here > l->high || goal < l->low || goal > l->high)
        return false;
    if (!state_is_valid(state))
        return false;

    bool already_opening = l->reported && strcmp(l->state, "Opening") == 0 && l->here == here;

    l->here = here;
    l->goal = goal;
    snprintf(l->state, sizeof l->state, "%s", state);
    l->reported = true;

    bool arrived_to_open = !already_opening && strcmp(state, "Opening") == 0 &&
                           l->route_count > 0 && l->routes[0].stops[0] == here;
    if (arrived_to_open) {
        pop_front(l);
        l->sent_goal = NO_FLOOR;
    }

    push_command(l);
    return l->active;
}

static bool apply_select(lift_t *l, const char *floor_text)
{
    int f;
    if (!l->reported || !floor_ordinal(floor_text, &f))
        return false;
    if (f < l->low || f > l->high)
        return false;
    if (!plan_stop(l, f, 0))
        return false;
    push_command(l);
    return l->active;
}

static lift_t *find_reported_lift(const char *name)
{
    for (int i = 0; i < CAR_SLOTS; i++) {
        if (lifts[i].active && lifts[i].reported && strcmp(lifts[i].name, name) == 0)
            return &lifts[i];
    }
    return NULL;
}

static bool apply_call(int fd, const char *name, const char *floor_text, const char *dir_text)
{
    int f;
    int want;

    if (!name_ok(name) || !floor_ordinal(floor_text, &f))
        return false;
    if (strcmp(dir_text, "UP") == 0)
        want = 1;
    else if (strcmp(dir_text, "DOWN") == 0)
        want = -1;
    else
        return false;

    bool accepted = false;
    lift_t *l = find_reported_lift(name);
    if (l != NULL && f >= l->low && f <= l->high) {
        bool at_end = (want == 1 && f == l->high) || (want == -1 && f == l->low);
        if (!at_end && plan_stop(l, f, want)) {
            push_command(l);
            accepted = l->active;
        }
    }
    return send_frame(fd, accepted ? "ACCEPTED" : "UNAVAILABLE");
}

static int tokenize(char *text, char *tok[], int max)
{
    int n = 0;
    char *piece;

    while ((piece = strsep(&text, " ")) != NULL) {
        if (n == max)
            return max + 1;
        tok[n++] = piece;
    }
    return n;
}

enum command {
    CMD_UNKNOWN,
    CMD_CAR,
    CMD_STATUS,
    CMD_SELECT,
    CMD_SERVICE,
    CMD_EMERGENCY,
    CMD_CALL
};

static enum command command_of(const char *word)
{
    static const struct {
        const char *word;
        enum command cmd;
    } table[] = {
        {"CAR", CMD_CAR},
        {"STATUS", CMD_STATUS},
        {"SELECT", CMD_SELECT},
        {"SERVICE", CMD_SERVICE},
        {"EMERGENCY", CMD_EMERGENCY},
        {"CALL", CMD_CALL},
    };

    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (strcmp(word, table[i].word) == 0)
            return table[i].cmd;
    }
    return CMD_UNKNOWN;
}

static bool process_message(int fd, lift_t **slot, uint64_t *serial, char *msg)
{
    char *tok[5];
    int n = tokenize(msg, tok, 4);
    bool ok = false;

    pthread_mutex_lock(&registry_lock);

    lift_t *l = *slot;
    if (l != NULL && (!l->active || l->serial != *serial)) {
        pthread_mutex_unlock(&registry_lock);
        return false;
    }

    switch (command_of(tok[0])) {
    case CMD_CAR: {
        int low, high;
        bool valid = l == NULL && n == 4 && name_ok(tok[1]) &&
                     floor_ordinal(tok[2], &low) && floor_ordinal(tok[3], &high) &&
                     low < high;
        if (valid) {
            l = enroll_lift(fd, tok[1], low, high);
            if (l != NULL) {
                *slot = l;
                *serial = l->serial;
                ok = true;
            }
        }
        break;
    }
    case CMD_STATUS:
        ok = l != NULL && n == 4 && apply_status(l, tok[1], tok[2], tok[3]);
        break;
    case CMD_SELECT:
        ok = l != NULL && n == 2 && apply_select(l, tok[1]);
        break;
    case CMD_SERVICE:
    case CMD_EMERGENCY:
        if (l != NULL && n == 1)
            drop_lift(l);
        ok = false;
        break;
    case CMD_CALL:
        ok = l == NULL && n == 4 && apply_call(fd, tok[1], tok[2], tok[3]);
        break;
    case CMD_UNKNOWN:
        break;
    }

    if (l != NULL && !l->active)
        ok = false;

    pthread_mutex_unlock(&registry_lock);
    return ok;
}

static bool fd_register(int fd)
{
    for (int i = 0; i < CONN_SLOTS; i++) {
        if (conn_table[i] == 0) {
            conn_table[i] = fd;
            return true;
        }
    }
    return false;
}

static bool fd_release(int fd)
{
    for (int i = 0; i < CONN_SLOTS; i++) {
        if (conn_table[i] == fd) {
            conn_table[i] = 0;
            return true;
        }
    }
    return false;
}

static void *connection_main(void *arg)
{
    int fd = (int)(intptr_t)arg;
    char buf[MAX_MSG_LEN + 1];
    lift_t *lift = NULL;
    uint64_t serial = 0;

    while (running) {
        int got = frame_recv(fd, buf, sizeof buf);
        if (got <= 0)
            break;
        if (!process_message(fd, &lift, &serial, buf))
            break;
    }

    pthread_mutex_lock(&registry_lock);
    if (lift != NULL && lift->active && lift->serial == serial)
        drop_lift(lift);
    bool owned = fd_release(fd);
    pthread_mutex_unlock(&registry_lock);

    if (owned)
        close(fd);
    return NULL;
}

static void spawn_connection(int cfd)
{
    pthread_mutex_lock(&registry_lock);
    bool tracked = fd_register(cfd);
    pthread_mutex_unlock(&registry_lock);

    if (!tracked) {
        close(cfd);
        return;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, connection_main, (void *)(intptr_t)cfd) != 0) {
        pthread_mutex_lock(&registry_lock);
        bool owned = fd_release(cfd);
        pthread_mutex_unlock(&registry_lock);
        if (owned)
            close(cfd);
        return;
    }
    pthread_detach(thread);
}

static void accept_loop(int listener)
{
    while (running) {
        int cfd = accept(listener, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR || errno == ECONNABORTED)
                continue;
            if (errno == EMFILE || errno == ENFILE)
                usleep(100000);
            continue;
        }
        spawn_connection(cfd);
    }
}

static void close_all_connections(void)
{
    pthread_mutex_lock(&registry_lock);
    for (int i = 0; i < CONN_SLOTS; i++) {
        if (conn_table[i] != 0) {
            shutdown(conn_table[i], SHUT_RDWR);
            close(conn_table[i]);
            conn_table[i] = 0;
        }
    }
    pthread_mutex_unlock(&registry_lock);
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    int listener = listen_socket();
    if (listener < 0)
        return 1;

    accept_loop(listener);

    close(listener);
    close_all_connections();
    return 0;
}