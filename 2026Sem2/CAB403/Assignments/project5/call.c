#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 3001
#define MAX_MSG_LEN 4096
#define UNREACHABLE_TEXT "Unable to connect to elevator system.\n"

static int open_connection(void)
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

static int shut_connection(int fd)
{
    shutdown(fd, SHUT_RDWR);
    return close(fd) < 0 ? -1 : 0;
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

static const char *direction_word(const char *arg)
{
    if (strcmp(arg, "up") == 0)
        return "UP";
    if (strcmp(arg, "down") == 0)
        return "DOWN";
    return NULL;
}

static int fail(const char *text)
{
    fputs(text, stderr);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s {car name} {floor} {up|down}\n", argv[0]);
        return 1;
    }

    const char *car = argv[1];
    const char *floor_arg = argv[2];
    int floor_value;

    if (!name_ok(car))
        return fail("Invalid car name.\n");
    if (!floor_ok(floor_arg, &floor_value))
        return fail("Invalid floor specified.\n");

    const char *direction = direction_word(argv[3]);
    if (direction == NULL)
        return fail("Invalid direction specified.\n");

    int fd = open_connection();
    if (fd < 0)
        return fail(UNREACHABLE_TEXT);

    struct timeval timeout = {5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    char request[96];
    int request_len = snprintf(request, sizeof request, "CALL %s %s %s", car, floor_arg, direction);
    if (frame_send(fd, request, (uint32_t)request_len) != 1) {
        shut_connection(fd);
        return fail(UNREACHABLE_TEXT);
    }

    char reply[MAX_MSG_LEN + 1];
    int reply_len = frame_recv(fd, reply, sizeof reply);
    shut_connection(fd);

    if (reply_len > 0) {
        if (strcmp(reply, "ACCEPTED") == 0) {
            printf("Car %s is arriving.\n", car);
            return 0;
        }
        if (strcmp(reply, "UNAVAILABLE") == 0) {
            printf("Sorry, that car is unavailable for this request.\n");
            return 0;
        }
    }
    return fail(UNREACHABLE_TEXT);
}