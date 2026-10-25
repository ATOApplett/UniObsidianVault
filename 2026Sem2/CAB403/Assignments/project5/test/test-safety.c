#include "shared.h"

#define DELAY 60000

static int failures;

static void wake(car_shared_mem *shared)
{
  pthread_cond_broadcast(&shared->cond);
  pthread_mutex_unlock(&shared->mutex);
  usleep(DELAY);
}

static void check(int condition, const char *description)
{
  printf("%s: %s\n", description, condition ? "passed" : "FAILED");
  if (!condition) {
    failures++;
  }
}

static void restore(car_shared_mem *shared)
{
  reset_shm(shared);
  pthread_cond_broadcast(&shared->cond);
  usleep(DELAY);
}

static pid_t launch_safety(void)
{
  pid_t pid = fork();
  if (pid == 0) {
    execlp("./safety", "./safety", "Test", NULL);
    _exit(127);
  }
  return pid;
}

int main(void)
{
  const char *shared_name = SHARED_MEMORY_PREFIX "Test";
  int fd;
  car_shared_mem *shared;
  pid_t safety_pid;

  printf("# The safety component must flush required diagnostic output if stdio is used\n\n");
  shm_unlink(shared_name);
  msg("Unable to access car Test.");
  (void)system("./safety Test");

  fd = shm_open(shared_name, O_CREAT | O_EXCL | O_RDWR, 0600);
  if ((fd < 0) || (ftruncate(fd, sizeof(car_shared_mem)) != 0)) {
    perror("shared memory");
    return 1;
  }
  shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shared == MAP_FAILED) {
    perror("mmap()");
    return 1;
  }
  init_shm(shared);
  safety_pid = launch_safety();
  usleep((SAFETY_HEARTBEAT_INTERVAL_MS + 50U) * 1000U);

  pthread_mutex_lock(&shared->mutex);
  {
    uint32_t first_heartbeat = shared->safety_heartbeat;
    check(first_heartbeat != 0U, "Safety heartbeat starts advancing");
    pthread_mutex_unlock(&shared->mutex);
    usleep((SAFETY_HEARTBEAT_INTERVAL_MS + 50U) * 1000U);
    pthread_mutex_lock(&shared->mutex);
    check(shared->safety_heartbeat != first_heartbeat,
          "Safety heartbeat continues advancing");
  }
  pthread_mutex_unlock(&shared->mutex);

  restore(shared);
  pthread_mutex_lock(&shared->mutex);
  strcpy(shared->status, "Closing");
  shared->door_obstruction = 1U;
  wake(shared);
  check(strcmp(shared->status, "Opening") == 0,
        "Door obstruction reverses closing doors");

  restore(shared);
  pthread_mutex_lock(&shared->mutex);
  shared->emergency_stop = 1U;
  wake(shared);
  check((shared->emergency_stop == 0U) && (shared->emergency_mode == 1U),
        "Emergency stop enters emergency mode");

  restore(shared);
  pthread_mutex_lock(&shared->mutex);
  shared->overload = 1U;
  wake(shared);
  check(shared->emergency_mode == 1U, "Overload enters emergency mode");

  restore(shared);
  pthread_mutex_lock(&shared->mutex);
  strcpy(shared->target_floor, "DEF");
  wake(shared);
  check(shared->emergency_mode == 1U, "Invalid target is rejected");

  restore(shared);
  pthread_mutex_lock(&shared->mutex);
  strcpy(shared->floor_request, "ABC");
  shared->floor_request_pending = 1U;
  wake(shared);
  check(shared->emergency_mode == 1U,
        "Invalid pending passenger request is rejected");

  restore(shared);
  pthread_mutex_lock(&shared->mutex);
  shared->floor_request_pending = 2U;
  wake(shared);
  check(shared->emergency_mode == 1U,
        "Non-binary request flag is rejected");

  restore(shared);
  pthread_mutex_lock(&shared->mutex);
  shared->service_mode = 16U;
  wake(shared);
  check(shared->emergency_mode == 1U, "Non-binary service mode is rejected");

  restore(shared);
  pthread_mutex_lock(&shared->mutex);
  strcpy(shared->status, "Closed");
  shared->door_obstruction = 1U;
  wake(shared);
  check(shared->emergency_mode == 1U,
        "Obstruction in an impossible state is rejected");

  kill(safety_pid, SIGINT);
  waitpid(safety_pid, NULL, 0);
  munmap(shared, sizeof(*shared));
  close(fd);
  shm_unlink(shared_name);

  if (failures != 0) {
    fprintf(stderr, "%d safety check(s) failed.\n", failures);
    return 1;
  }
  printf("\nTests completed.\n");
  return 0;
}
