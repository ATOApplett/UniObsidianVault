#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 3000
#define SERVER_IP "127.0.0.1"

int is_valid_flr(const char *flr) {
    if (flr == NULL) return 0;
    size_t len = strlen(flr);
    if (len < 1 || len > 3) return 0;

    if (flr[0] == 'B') {
        if (len < 2) return 0;
        for (size_t i = 1; i < len; i++) {
            if (!isdigit((unsigned char)flr[i])) return 0;
        }
        int num = atoi(flr + 1);
        return num >= 1 && num <= 99;
    }

    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)flr[i])) return 0;
    }
    int num = atoi(flr);
    return num >= 1 && num <= 999;
}

static int send_all(int sock, const void *buf, size_t len) {
    const char *p = buf;
    while (len > 0) {
        ssize_t n = send(sock, p, len, 0);
        if (n <= 0) return -1;
        p += n;
        len -= n;
    }
    return 0;
}

static int recv_all(int sock, void *buf, size_t len) {
    char *p = buf;
    while (len > 0) {
        ssize_t n = recv(sock, p, len, 0);
        if (n <= 0) return -1;
        p += n;
        len -= n;
    }
    return 0;
}

// 16-bit length prefix in network byte order
int send_message(int sock, const char *message) {
    uint16_t len = (uint16_t)strlen(message);
    uint16_t net_len = htons(len);
    if (send_all(sock, &net_len, sizeof(net_len)) == -1) return -1;
    if (send_all(sock, message, len) == -1) return -1;
    return 0;
}

int receive_response(int sock, char *response, size_t max_len) {
    uint16_t net_len;
    if (recv_all(sock, &net_len, sizeof(net_len)) == -1) return -1;
    uint16_t len = ntohs(net_len);
    if (len >= max_len) return -1;
    if (recv_all(sock, response, len) == -1) return -1;
    response[len] = '\0';
    return 0;
}

void connect_to_controller(const char *src_flr, const char *dstn_flr) {
    struct sockaddr_in addr;
    char message[256];
    char response[256];

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Unable to connect to elevator system.\n");
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0 ||
        connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        printf("Unable to connect to elevator system.\n");
        close(sock);
        return;
    }

    snprintf(message, sizeof(message), "CALL %s %s", src_flr, dstn_flr);
    if (send_message(sock, message) == -1 ||
        receive_response(sock, response, sizeof(response)) == -1) {
        printf("Unable to connect to elevator system.\n");
        close(sock);
        return;
    }

    if (strcmp(response, "UNAVAILABLE") == 0) {
        printf("Sorry, no car is available to take this request.\n");
    } else {
        printf("%s is arriving.\n", response);   // same as the working version
    }
    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s {car name} {floor} {up|down}\n", argv[0]);
        return 1;
    }

    const char *src_flr = argv[1];
    const char *dstn_flr = argv[2];

    if (!is_valid_flr(src_flr) || !is_valid_flr(dstn_flr)) {
        printf("Invalid floor(s) specified.\n");
        return 1;
    }
    if (strcmp(src_flr, dstn_flr) == 0) {
        printf("You are already on that floor!\n");
        return 0;
    }

    connect_to_controller(src_flr, dstn_flr);
    return 0;
}