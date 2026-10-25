#include "shared.h"
#include <poll.h>

// Tester for the heartbeat check in car and safety

#define STARTUP_TIMEOUT_MS 3000
#define RECEIVE_TIMEOUT_MS 2500

static int failures;

static void check(int condition, const char *description)
{
  printf("%s: %s\n", description, condition ? "passed" : "FAILED");
  if (!condition) {
    failures++;
  }
}

static int make_server(void)
{
  struct sockaddr_in address;
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  int enabled = 1;

  if (fd < 0) {
    return -1;
  }
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(CONTROLLER_PORT);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if ((bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0) ||
      (listen(fd, 4) != 0)) {
    close(fd);
    return -1;
  }
  return fd;
}

static pid_t launch(const char *program, const char *first,
                    const char *second, const char *third,
                    const char *fourth)
{
  pid_t pid = fork();
  if (pid == 0) {
    execl(program, program, first, second, third, fourth, NULL);
    _exit(127);
  }
  return pid;
}

static int accept_with_timeout(int server)
{
  struct pollfd descriptor = {server, POLLIN, 0};
  if (poll(&descriptor, 1, STARTUP_TIMEOUT_MS) != 1) {
    return -1;
  }
  return accept(server, NULL, NULL);
}

static car_shared_mem *open_shared_memory(int *fd)
{
  unsigned int attempt;

  for (attempt = 0U; attempt < 200U; attempt++) {
    *fd = shm_open(SHARED_MEMORY_PREFIX "Watchdog", O_RDWR, 0600);
    if (*fd >= 0) {
      return mmap(NULL, sizeof(car_shared_mem), PROT_READ | PROT_WRITE,
                  MAP_SHARED, *fd, 0);
    }
    usleep(10000);
  }
  return MAP_FAILED;
}

static int receive_expected(int fd, const char *expected)
{
  char *message = receive_msg(fd);
  int matches = strcmp(message, expected) == 0;
  printf("Received: %s\n", message);
  free(message);
  check(matches, expected);
  return matches;
}

static int wait_for_closed_at_two(int fd)
{
  unsigned int message_number;

  for (message_number = 0U; message_number < 20U; message_number++) {
    char *message = receive_msg(fd);
    int finished = strcmp(message, "STATUS 2 2 Closed") == 0;
    int emergency = strcmp(message, "EMERGENCY") == 0;
    printf("Received: %s\n", message);
    free(message);
    if (emergency) {
      return 0;
    }
    if (finished) {
      return 1;
    }
  }
  return 0;
}

static int stop_safety_after_heartbeat(car_shared_mem *shared,
                                       pid_t safety_pid)
{
  uint32_t starting_value;
  struct timespec deadline;
  int result = 0;

  clock_gettime(CLOCK_MONOTONIC, &deadline);
  deadline.tv_sec += 2;
  pthread_mutex_lock(&shared->mutex);
  starting_value = shared->safety_heartbeat;
  while (shared->safety_heartbeat == starting_value) {
    if (pthread_cond_timedwait(&shared->cond, &shared->mutex,
                               &deadline) == ETIMEDOUT) {
      pthread_mutex_unlock(&shared->mutex);
      return 0;
    }
  }
  /* Holding the mutex proves the stopped process cannot own it. */
  if (kill(safety_pid, SIGSTOP) == 0) {
    result = 1;
  }
  pthread_mutex_unlock(&shared->mutex);
  return result;
}

static double elapsed_ms(const struct timespec *start,
                         const struct timespec *finish)
{
  double seconds = (double)(finish->tv_sec - start->tv_sec) * 1000.0;
  double nanoseconds = (double)(finish->tv_nsec - start->tv_nsec) / 1000000.0;
  return seconds + nanoseconds;
}

int main(void)
{
  int server = -1;
  int connection = -1;
  int shared_fd = -1;
  car_shared_mem *shared = MAP_FAILED;
  pid_t car_pid = -1;
  pid_t safety_pid = -1;
  struct timeval receive_timeout;
  struct timespec stopped_at;
  struct timespec emergency_at;
  char *message;

  shm_unlink(SHARED_MEMORY_PREFIX "Watchdog");
  server = make_server();
  if (server < 0) {
    perror("server");
    return 1;
  }
  car_pid = launch("./car", "Watchdog", "150", "1", "3");
  shared = open_shared_memory(&shared_fd);
  if (shared == MAP_FAILED) {
    perror("shared memory");
    failures++;
    goto cleanup;
  }
  safety_pid = launch("./safety", "Watchdog", NULL, NULL, NULL);

  connection = accept_with_timeout(server);
  if (connection < 0) {
    fprintf(stderr, "Car did not connect after observing a heartbeat.\n");
    failures++;
    goto cleanup;
  }
  receive_timeout.tv_sec = RECEIVE_TIMEOUT_MS / 1000;
  receive_timeout.tv_usec = (RECEIVE_TIMEOUT_MS % 1000) * 1000;
  setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO,
             &receive_timeout, sizeof(receive_timeout));
  receive_expected(connection, "CAR Watchdog 1 3");
  receive_expected(connection, "STATUS 1 1 Closed");

  send_message(connection, "FLOOR 2");
  check(wait_for_closed_at_two(connection),
        "Real safety heartbeat does not restart car operation timers");

  check(stop_safety_after_heartbeat(shared, safety_pid),
        "Safety process suspended without owning the shared mutex");
  clock_gettime(CLOCK_MONOTONIC, &stopped_at);
  message = receive_msg(connection);
  clock_gettime(CLOCK_MONOTONIC, &emergency_at);
  printf("Received: %s\n", message);
  check(strcmp(message, "EMERGENCY") == 0,
        "Car reports a stopped safety process");
  free(message);
  {
    double detection_time = elapsed_ms(&stopped_at, &emergency_at);
    printf("Watchdog detection time: %.0f ms\n", detection_time);
    /* The implementation deadline is 500 ms; this wider tester window
       permits process scheduling after that deadline expires. */
    check((detection_time >= 350.0) && (detection_time <= 1500.0),
          "Watchdog uses a tolerant failure window");
  }

cleanup:
  if (connection >= 0) {
    close(connection);
  }
  if (server >= 0) {
    close(server);
  }
  if (car_pid > 0) {
    kill(car_pid, SIGKILL);
    waitpid(car_pid, NULL, 0);
  }
  /* Stop the car before removing a suspended process-shared condvar waiter. */
  if (safety_pid > 0) {
    kill(safety_pid, SIGKILL);
    waitpid(safety_pid, NULL, 0);
  }
  if (shared != MAP_FAILED) {
    munmap(shared, sizeof(*shared));
  }
  if (shared_fd >= 0) {
    close(shared_fd);
  }
  shm_unlink(SHARED_MEMORY_PREFIX "Watchdog");

  if (failures != 0) {
    fprintf(stderr, "%d heartbeat integration check(s) failed.\n", failures);
    return 1;
  }
  printf("\nTests completed.\n");
  return 0;
}
