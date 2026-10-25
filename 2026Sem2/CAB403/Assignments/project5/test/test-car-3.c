#include "shared.h"

// Tester for car (with controller. No shared memory tests.)

#define DELAY 50000 // 50ms
#define MILLISECOND 1000 // 1ms

pid_t car(const char *, const char *, const char *, const char *);
void cleanup(pid_t);
void server_init();
void test_recv(int, const char *);
void *simulate_heartbeat(void *);

int shm_fd;
static car_shared_mem *shm;

int server_fd;
pthread_t heartbeat_tid;
int heartbeat_cancel = 0;

int main()
{
  shm_unlink("/cab403_car_Test"); // Remove shm object if it exists

  pid_t p;

  // Testing with server launched before car
  server_init();

  p = car("Test", "1", "8", "20");
  
  int fd;
  fd = accept(server_fd, NULL, NULL);
  test_recv(fd, "RECV: CAR Test 1 8");

  // Receive first update (should come immediately)
  test_recv(fd, "RECV: STATUS 1 1 Closed");

  // A normal-operation floor button request is forwarded to the controller.
  pthread_mutex_lock(&shm->mutex);
  strcpy(shm->floor_request, "6");
  shm->floor_request_pending = 1U;
  pthread_cond_broadcast(&shm->cond);
  pthread_mutex_unlock(&shm->mutex);
  test_recv(fd, "RECV: SELECT 6");
  usleep(DELAY);
  pthread_mutex_lock(&shm->mutex);
  if (shm->floor_request_pending != 0U) {
    fprintf(stderr, "Floor request was not acknowledged by the car.\n");
    exit(1);
  }
  pthread_mutex_unlock(&shm->mutex);

  // Send the car up to floor 4
  send_message(fd, "FLOOR 4");
  {
    msg("RECV: STATUS 1 4 Closed (or Between 1 4)");
    char *m = receive_msg(fd);
    printf("RECV: %s\n", m);
    if (strstr(m, "Closed")!=NULL) { // expect Between
      test_recv(fd, "RECV: STATUS 1 4 Between");
    }
    free(m);
  }
  test_recv(fd, "RECV: STATUS 2 4 Between");
  test_recv(fd, "RECV: STATUS 3 4 Between");
  test_recv(fd, "RECV: STATUS 4 4 Opening");
  test_recv(fd, "RECV: STATUS 4 4 Open");
  test_recv(fd, "RECV: STATUS 4 4 Closing");
  test_recv(fd, "RECV: STATUS 4 4 Closed");

  // Send the car the same floor to get it to open its doors again
  send_message(fd, "FLOOR 4");
  test_recv(fd, "RECV: STATUS 4 4 Opening");
  test_recv(fd, "RECV: STATUS 4 4 Open");
  test_recv(fd, "RECV: STATUS 4 4 Closing");
  test_recv(fd, "RECV: STATUS 4 4 Closed");

  // Send the car down to floor 2
  send_message(fd, "FLOOR 2");
  {
    msg("RECV: STATUS 4 2 Closed (or Between 4 2)");
    char *m = receive_msg(fd);
    printf("RECV: %s\n", m);
    if (strstr(m, "Closed")!=NULL) { // expect Between
      test_recv(fd, "RECV: STATUS 4 2 Between");
    }
    free(m);
  }
  test_recv(fd, "RECV: STATUS 3 2 Between");
  test_recv(fd, "RECV: STATUS 2 2 Opening");
  test_recv(fd, "RECV: STATUS 2 2 Open");
  test_recv(fd, "RECV: STATUS 2 2 Closing");
  test_recv(fd, "RECV: STATUS 2 2 Closed");

  close(fd);
  close(server_fd);

  cleanup(p);
  printf("\nTests completed.\n");
}

void test_recv(int fd, const char *t)
{
  char *m = receive_msg(fd);
  msg(t);
  printf("RECV: %s\n", m);
  if (strncmp(t, "RECV: ", 6) != 0 || strcmp(m, t + 6) != 0) exit(1);
  free(m);
}

void cleanup(pid_t p)
{
  pthread_mutex_lock(&shm->mutex);
  heartbeat_cancel = 1;
  pthread_cond_broadcast(&shm->cond);
  pthread_mutex_unlock(&shm->mutex);
  pthread_join(heartbeat_tid, NULL);

  munmap(shm, sizeof(car_shared_mem));
  close(shm_fd);
  kill(p, SIGINT);
  waitpid(p, NULL, 0);
  shm_unlink("/cab403_car_Test");
}

pid_t car(const char *name, const char *lowest_floor, const char *highest_floor, const char *delay)
{
  pid_t pid = fork();
  if (pid == 0) {
    execlp("./car", "./car", name, delay, lowest_floor, highest_floor, NULL);
  }
  usleep(DELAY);
  shm_fd = shm_open("/cab403_car_Test", O_RDWR, 0666);
  shm = mmap(0, sizeof(*shm), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  pthread_create(&heartbeat_tid, NULL, simulate_heartbeat, NULL);

  return pid;
}

void server_init()
{
  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons(CONTROLLER_PORT);
  a.sin_addr.s_addr = htonl(INADDR_ANY);

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt_enable = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable));
  if (bind(server_fd, (const struct sockaddr *)&a, sizeof(a)) == -1) {
    perror("bind()");
    exit(1);
  }

  listen(server_fd, 10);
}

void *simulate_heartbeat(void *_)
{
  (void)_;
  for (;;) {
    usleep(SAFETY_HEARTBEAT_INTERVAL_MS * MILLISECOND);
    pthread_mutex_lock(&shm->mutex);
    if (heartbeat_cancel) {
      pthread_mutex_unlock(&shm->mutex);
      break;
    }
    shm->safety_heartbeat++;
    pthread_cond_broadcast(&shm->cond);
    pthread_mutex_unlock(&shm->mutex);
  }
  return NULL;
}
