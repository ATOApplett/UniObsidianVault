#include "shared.h"

static int failures;

static void check(int condition, const char *description)
{
  if (!condition) {
    fprintf(stderr, "FAILED: %s\n", description);
    failures++;
  }
}

static void show_state(car_shared_mem *shared)
{
  printf("Current state: {%s, %s, %s, %s, %u, %u, %u, %u, %u, %u, %u, %u, %u}\n",
         shared->status,
         shared->current_floor,
         shared->target_floor,
         shared->floor_request,
         shared->open_button,
         shared->close_button,
         shared->floor_request_pending,
         shared->door_obstruction,
         shared->overload,
         shared->emergency_stop,
         shared->service_mode,
         shared->emergency_mode,
         shared->safety_heartbeat);
}

int main(void)
{
  const char *shared_name = SHARED_MEMORY_PREFIX "Test";
  int fd;
  car_shared_mem *shared;

  shm_unlink(shared_name);
  msg("Unable to access car Test.");
  (void)system("./internal open Test");

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

  (void)system("./internal open Test");
  check(shared->open_button == 1U, "open sets open_button");
  (void)system("./internal close Test");
  check(shared->close_button == 1U, "close sets close_button");
  (void)system("./internal stop Test");
  check(shared->emergency_stop == 1U, "stop sets emergency_stop");

  (void)system("./internal floor Test 7");
  check((shared->floor_request_pending == 1U) &&
        (strcmp(shared->floor_request, "7") == 0),
        "floor writes the normal-operation request mailbox");
  msg("A floor request is already pending.");
  (void)system("./internal floor Test B1");
  check(strcmp(shared->floor_request, "7") == 0,
        "a pending request is not overwritten");

  shared->floor_request_pending = 0U;
  (void)system("./internal service_on Test");
  check((shared->service_mode == 1U) && (shared->emergency_mode == 0U),
        "service_on changes both mode fields");
  msg("Floor selection only allowed in normal operation.");
  (void)system("./internal floor Test 4");

  strcpy(shared->current_floor, "1");
  strcpy(shared->target_floor, "1");
  strcpy(shared->status, "Closed");
  (void)system("./internal up Test");
  check(strcmp(shared->target_floor, "2") == 0,
        "up chooses the next floor in service mode");
  strcpy(shared->target_floor, "1");
  (void)system("./internal down Test");
  check(strcmp(shared->target_floor, "B1") == 0,
        "down crosses from floor 1 to B1");

  (void)system("./internal service_off Test");
  check(shared->service_mode == 0U, "service_off leaves service mode");
  msg("Operation only allowed in service mode.");
  (void)system("./internal up Test");
  msg("Invalid floor specified.");
  (void)system("./internal floor Test 01");
  msg("Invalid operation.");
  (void)system("./internal launch Test");

  show_state(shared);
  pthread_cond_destroy(&shared->cond);
  pthread_mutex_destroy(&shared->mutex);
  munmap(shared, sizeof(*shared));
  close(fd);
  shm_unlink(shared_name);

  if (failures != 0) {
    fprintf(stderr, "%d internal control check(s) failed.\n", failures);
    return 1;
  }
  printf("\nTests completed.\n");
  return 0;
}
