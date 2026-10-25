#include "shared.h"

// Tester for hall call component

#define DELAY 50000

static void *server(void *unused);

int main(void)
{
  pthread_t thread;

  msg("Unable to connect to elevator system.");
  (void)system("./call Test B1 up");

  if (pthread_create(&thread, NULL, server, NULL) != 0) {
    perror("pthread_create()");
    return 1;
  }
  usleep(DELAY);

  msg("RECV: CALL Test B21 UP : Car Test is arriving.");
  (void)system("./call Test B21 up");
  usleep(DELAY);

  msg("RECV: CALL Express 416 DOWN : Sorry, that car is unavailable for this request.");
  (void)system("./call Express 416 down");
  usleep(DELAY);

  msg("Invalid car name.");
  (void)system("./call Bad/name 4 up");
  msg("Invalid floor specified.");
  (void)system("./call Test L4 up");
  msg("Invalid floor specified.");
  (void)system("./call Test B100 down");
  msg("Invalid floor specified.");
  (void)system("./call Test 1000 up");
  msg("Invalid floor specified.");
  (void)system("./call Test 01 up");
  msg("Invalid direction specified.");
  (void)system("./call Test 4 sideways");

  (void)pthread_join(thread, NULL);
  printf("\nTests completed.\n");
  return 0;
}

static void *server(void *unused)
{
  struct sockaddr_in address;
  int listener;
  int enable = 1;
  int fd;
  char *message;

  (void)unused;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(CONTROLLER_PORT);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0) {
    perror("socket()");
    exit(1);
  }
  (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
  if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0) {
    perror("bind()");
    exit(1);
  }
  if (listen(listener, 4) != 0) {
    perror("listen()");
    exit(1);
  }

  fd = accept(listener, NULL, NULL);
  message = receive_msg(fd);
  printf("RECV: %s : ", message);
  fflush(stdout);
  free(message);
  send_message(fd, "ACCEPTED");
  close(fd);

  fd = accept(listener, NULL, NULL);
  message = receive_msg(fd);
  printf("RECV: %s : ", message);
  fflush(stdout);
  free(message);
  send_message(fd, "UNAVAILABLE");
  close(fd);
  close(listener);
  return NULL;
}
