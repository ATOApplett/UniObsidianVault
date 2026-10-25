#include "shared.h"

// Tester for car (Testing controller, shared memory and service mode.)

#define DELAY 50000 // 50ms
#define MILLISECOND 1000 // 1ms

pid_t car(const char *, const char *, const char *, const char *);
void displaycond(car_shared_mem *);
void cleanup(pid_t);
void server_init();
void test_recv(int, const char *);
void *simulate_heartbeat(void *);

int server_fd;
int shm_fd;
static car_shared_mem *shm;

pthread_t heartbeat_tid;
int heartbeat_cancel = 0;

int main()
{
  printf("# Some adjacent Closed and Between status updates may arrive in\n");
  printf("# either valid order. This tester allows the documented alternatives;\n");
  printf("# other missing transitions indicate a problem.\n");
  shm_unlink("/cab403_car_Test"); // Remove shm object if it exists

  pid_t p;
  int fcntl_flags;
  char tmp_recv_buffer;
  int recv_returnval;

  // Testing with car launched before server
  p = car("Test", "B45", "B30", "30"); // This waits DELAY (50ms) after creating the process

  msg("Current state: {B45, B45, Closed, 0, 0, 0, 0, 0, 0, 0}");
  displaycond(shm);
  usleep(100 * MILLISECOND);

  // The car should be repeatedly trying to connect every 30ms, which means it
  // should make another attempt within 30ms

  server_init();
  
  int fd;
  fd = accept(server_fd, NULL, NULL);
  test_recv(fd, "RECV: CAR Test B45 B30");

  // Receive first update (should come immediately)
  test_recv(fd, "RECV: STATUS B45 B45 Closed");

  // Send the elevator up to floor B43
  send_message(fd, "FLOOR B43");

  {
    msg("RECV: STATUS B45 B43 Closed (or Between B45 B43)");
    char *m = receive_msg(fd);
    printf("RECV: %s\n", m);
    if (strstr(m, "Closed")!=NULL) { // expect Between
      test_recv(fd, "RECV: STATUS B45 B43 Between");
    }
    free(m);
  }

  test_recv(fd, "RECV: STATUS B44 B43 Between");
  test_recv(fd, "RECV: STATUS B43 B43 Opening");
  msg("Current state: {B43, B43, Opening, 0, 0, 0, 0, 0, 0, 0}");
  displaycond(shm);
  test_recv(fd, "RECV: STATUS B43 B43 Open");
  test_recv(fd, "RECV: STATUS B43 B43 Closing");
  test_recv(fd, "RECV: STATUS B43 B43 Closed");
  // Send the elevator up to floor B41
  send_message(fd, "FLOOR B41");
  {
    msg("RECV: STATUS B43 B41 Closed (or Between B43 B41)");
    char *m = receive_msg(fd);
    printf("RECV: %s\n", m);
    if (strstr(m, "Closed")!=NULL) { // expect Between
      test_recv(fd, "RECV: STATUS B43 B41 Between");
    }
    free(m);
  }
  // Someone hits the open button
  pthread_mutex_lock(&shm->mutex);
  shm->open_button = 1;
  pthread_cond_broadcast(&shm->cond);
  pthread_mutex_unlock(&shm->mutex);

  // Door should not open (because the car is between floors)
  
  test_recv(fd, "RECV: STATUS B42 B41 Between");
  test_recv(fd, "RECV: STATUS B41 B41 Opening");
  test_recv(fd, "RECV: STATUS B41 B41 Open");
  test_recv(fd, "RECV: STATUS B41 B41 Closing");
  test_recv(fd, "RECV: STATUS B41 B41 Closed");

  // Put the elevator into individual service mode
  pthread_mutex_lock(&shm->mutex);
  shm->service_mode = 1;
  pthread_cond_broadcast(&shm->cond);
  pthread_mutex_unlock(&shm->mutex);

  test_recv(fd, "RECV: SERVICE");

  usleep(20 * MILLISECOND);
  // Move the car to floor B40 manually
  pthread_mutex_lock(&shm->mutex);
  strcpy(shm->target_floor, "B40");
  pthread_cond_broadcast(&shm->cond);
  pthread_mutex_unlock(&shm->mutex);
  usleep(80 * MILLISECOND);

  // There should be no updates coming from
  // the car any more.

  // Set the socket to non-blocking mode and check
  fcntl_flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, fcntl_flags | O_NONBLOCK);
  recv_returnval = recv(fd, &tmp_recv_buffer, 1, 0);
  msg("Call to recv returned (0 or -1)");
  printf("Call to recv returned %d\n", recv_returnval);
  if (recv_returnval == -1) {
    msg("recv(): Resource temporarily unavailable");
    perror("recv()");
  }
  close(fd);

  // Take the car out of individual service mode
  pthread_mutex_lock(&shm->mutex);
  shm->service_mode = 0;
  pthread_cond_broadcast(&shm->cond);
  pthread_mutex_unlock(&shm->mutex);

  // It reconnects after observing a fresh safety heartbeat.
  fd = accept(server_fd, NULL, NULL);
  test_recv(fd, "RECV: CAR Test B45 B30");
  test_recv(fd, "RECV: STATUS B40 B40 Closed");

  // Put the car into emergency mode by shutting off the safety heartbeat
  pthread_mutex_lock(&shm->mutex);
  heartbeat_cancel = 1;
  pthread_mutex_unlock(&shm->mutex);
  pthread_join(heartbeat_tid, NULL);
  msg("Safety system disconnected! Entering emergency mode.");
  usleep((SAFETY_HEARTBEAT_TIMEOUT_MS + 200U) * MILLISECOND);

  test_recv(fd, "RECV: EMERGENCY");

  // There should be no updates coming from
  // the car any more.

  // Set the socket to non-blocking mode and check
  fcntl_flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, fcntl_flags | O_NONBLOCK);
  recv_returnval = recv(fd, &tmp_recv_buffer, 1, 0);
  msg("Call to recv returned (0 or -1)");
  printf("Call to recv returned %d\n", recv_returnval);
  if (recv_returnval == -1) {
    msg("recv(): Resource temporarily unavailable");
    perror("recv()");
  }
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

void displaycond(car_shared_mem *s)
{
  pthread_mutex_lock(&s->mutex);
  printf("Current state: {%s, %s, %s, %d, %d, %d, %d, %d, %d, %d}\n",
    s->current_floor,
    s->target_floor,
    s->status,
    s->open_button,
    s->close_button,
    s->door_obstruction,
    s->overload,
    s->emergency_stop,
    s->service_mode,
    s->emergency_mode
  );
  pthread_mutex_unlock(&s->mutex);
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
