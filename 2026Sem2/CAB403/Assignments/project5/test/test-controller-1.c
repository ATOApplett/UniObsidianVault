#include "shared.h"

#define START_DELAY 100000

static pid_t start_controller(void)
{
  pid_t pid = fork();
  if (pid == 0) {
    execlp("./controller", "./controller", NULL);
    _exit(127);
  }
  usleep(START_DELAY);
  return pid;
}

static int connect_client(void)
{
  struct sockaddr_in address;
  struct timeval timeout = {3, 0};
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(CONTROLLER_PORT);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if ((fd < 0) ||
      (connect(fd, (const struct sockaddr *)&address, sizeof(address)) != 0)) {
    perror("connect()");
    exit(1);
  }
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  return fd;
}

static int register_car(const char *registration, const char *status)
{
  int fd = connect_client();
  send_message(fd, registration);
  send_message(fd, status);
  usleep(20000);
  return fd;
}

static void expect_message(int fd, const char *expected)
{
  char *actual = receive_msg(fd);
  msg(expected);
  printf("%s\n", actual);
  if (strcmp(actual, expected) != 0) {
    fprintf(stderr, "Expected '%s', received '%s'.\n", expected, actual);
    free(actual);
    exit(1);
  }
  free(actual);
}

static void expect_call(const char *request, const char *expected)
{
  int fd = connect_client();
  send_message(fd, request);
  expect_message(fd, expected);
  close(fd);
}

static void stop_controller(pid_t pid)
{
  kill(pid, SIGINT);
  waitpid(pid, NULL, 0);
}

int main(void)
{
  pid_t controller = start_controller();
  int alpha;

  expect_call("CALL Alpha 1 UP", "UNAVAILABLE");

  alpha = register_car("CAR Alpha B1 4", "STATUS B1 B1 Closed");
  expect_call("CALL Alpha 1 UP", "ACCEPTED");
  expect_message(alpha, "FLOOR 1");

  send_message(alpha, "STATUS 1 1 Opening");
  send_message(alpha, "SELECT 2");
  expect_message(alpha, "FLOOR 2");
  send_message(alpha, "STATUS 2 2 Opening");

  expect_call("CALL Alpha 4 UP", "UNAVAILABLE");
  expect_call("CALL Alpha B1 DOWN", "UNAVAILABLE");
  expect_call("CALL Other 2 UP", "UNAVAILABLE");

  close(alpha);
  stop_controller(controller);
  printf("\nTests completed.\n");
  return 0;
}
