#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define PORT 3000
#define MAX_CARS 10
#define MAX_QUEUE 20
#define BUFFER_SIZE 256


typedef enum { UP, DOWN, NONE } Direction;

typedef struct {
    int sockfd;
    char name[BUFFER_SIZE];
    char low_flr[4];
    char high_flr[4];
    char cur_flr[4];
    char dstn_flr[4];
    char status[BUFFER_SIZE];
    int active;
    int queue[MAX_QUEUE];
    Direction directions[MAX_QUEUE];
    int queue_size;
} Car;

Car cars[MAX_CARS];
int num_cars = 0;
pthread_mutex_t cars_mutex = PTHREAD_MUTEX_INITIALIZER;
void send_message(int sockfd, const char *msg) {
    uint32_t len = htonl(strlen(msg));
    if (write(sockfd, &len, sizeof(len)) == -1 || write(sockfd, msg, strlen(msg)) == -1) {
        perror("write()");
    }
}

char *receive_msg(int sockfd) {
    uint32_t nlen;
    if (read(sockfd, &nlen, sizeof(nlen)) <= 0) return NULL;
    uint32_t len = ntohl(nlen);
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    if (read(sockfd, buf, len) <= 0) {
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

int flr_to_int(const char *label) {
    return label[0] == 'B' ? -atoi(label + 1) : atoi(label);
}

void send_flr(Car *car, int flr) {
    char msg[BUFFER_SIZE];
    if (flr < 0) snprintf(msg, sizeof(msg), "FLOOR B%d", -flr);
    else snprintf(msg, sizeof(msg), "FLOOR %d", flr);
    send_message(car->sockfd, msg);
}

int flr_exists(Car *car, int flr) {
    for (int i = 0; i < car->queue_size; i++) {
        if (car->queue[i] == flr) return 1;
    }
    return 0;
}

void ins_flrs(Car *car, int from_flr, int to_flr, Direction direction) {
    if (flr_exists(car, from_flr) || flr_exists(car, to_flr)) return;

    if (car->queue_size == 0) {
        car->queue[0] = from_flr;
        car->directions[0] = direction;
        car->queue[1] = to_flr;
        car->directions[1] = direction;
        car->queue_size = 2;
        return;
    }

    car->queue[car->queue_size] = from_flr;
    car->directions[car->queue_size] = direction;
    car->queue[car->queue_size + 1] = to_flr;
    car->directions[car->queue_size + 1] = direction;
    car->queue_size += 2;
}

void register_car(int sockfd, char *msg) {
    char name[BUFFER_SIZE], lowest[4], highest[4];
    sscanf(msg, "CAR %255s %3s %3s", name, lowest, highest);

    pthread_mutex_lock(&cars_mutex);
    if (num_cars < MAX_CARS) {
        Car *car = &cars[num_cars++];
        car->sockfd = sockfd;
        strcpy(car->name, name);
        strcpy(car->low_flr, lowest);
        strcpy(car->high_flr, highest);
        strcpy(car->status, "Closed");
        car->active = 1;
        car->queue_size = 0;

        printf("Car registered: %s (Floors: %s to %s)\n", name, lowest, highest);
    } else {
        printf("Max number of cars reached!\n");
    }
    pthread_mutex_unlock(&cars_mutex);
}

void handle_call(int clientfd, char *msg) {
    char from[4], to[4];
    sscanf(msg, "CALL %3s %3s", from, to);
    int from_flr = flr_to_int(from);
    int to_flr = flr_to_int(to);
    int low_req = from_flr < to_flr ? from_flr : to_flr;
    int high_req = from_flr < to_flr ? to_flr : from_flr;
    pthread_mutex_lock(&cars_mutex);

    int found = 0;
    for (int i = 0; i < num_cars; i++) {
        if (flr_to_int(cars[i].low_flr) <= low_req && flr_to_int(cars[i].high_flr) >= high_req &&
            cars[i].active && cars[i].queue_size + 2 <= MAX_QUEUE) {
            Direction direction = (from_flr < to_flr) ? UP : DOWN;
            ins_flrs(&cars[i], from_flr, to_flr, direction);
            if (strcmp(cars[i].status, "Closed") == 0) {
                send_flr(&cars[i], from_flr);
            }
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "CAR %s", cars[i].name);
            send_message(clientfd, response);
            found = 1;
            break;
        }
    }

    if (!found) {
        send_message(clientfd, "UNAVAILABLE");
    }
    pthread_mutex_unlock(&cars_mutex);
}

void update_car_status(int car_index, char *msg) {
    Car *car = &cars[car_index];
    sscanf(msg, "STATUS %255s %3s %3s", car->status, car->cur_flr, car->dstn_flr);
    printf("Received status: %s, current: %s, destination: %s\n", 
           car->status, car->cur_flr, car->dstn_flr);
    if (strcmp(car->status, "Opening") == 0 && flr_to_int(car->cur_flr) == flr_to_int(car->dstn_flr)) {
            send_flr(car, car->queue[0]);
            printf("Sent next floor to car %s: %d\n", car->name, car->queue[0]);
            for (int i = 1; i < car->queue_size; i++) {
                car->queue[i - 1] = car->queue[i];
            car->queue_size--;
            printf("Updated queue for car %s, new queue size: %d\n", car->name, car->queue_size);
        }
    }
}

void *client_handler(void *arg) {
    int clientfd = *(int *)arg;
    free(arg);

    char *msg;
    while ((msg = receive_msg(clientfd)) != NULL) {
        if (strncmp(msg, "CAR", 3) == 0) {
            register_car(clientfd, msg);
        } else if (strncmp(msg, "CALL", 4) == 0) {
            handle_call(clientfd, msg);
        } else if (strncmp(msg, "STATUS", 6) == 0) {
            pthread_mutex_lock(&cars_mutex);
            for (int i = 0; i < num_cars; i++) {
                if (cars[i].sockfd == clientfd) {
                    update_car_status(i, msg);
                    break;
                }
            }
            pthread_mutex_unlock(&cars_mutex);
        }
        free(msg);
    }

    close(clientfd);
    return NULL;
}

int main() {
    signal(SIGPIPE, SIG_IGN); 
    int serverfd, *clientfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd == -1) {
        perror("socket()");
        exit(1);
    }

    int opt = 1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind()");
        exit(1);
    }

    if (listen(serverfd, 10) == -1) {
        perror("listen()");
        exit(1);
    }

    printf("Controller running on port %d\n", PORT);

    while (1) {
        clientfd = malloc(sizeof(int));
        *clientfd = accept(serverfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (*clientfd == -1) {
            perror("accept()");
            free(clientfd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, clientfd);
        pthread_detach(tid);
    }

    close(serverfd);
    return 0;
}