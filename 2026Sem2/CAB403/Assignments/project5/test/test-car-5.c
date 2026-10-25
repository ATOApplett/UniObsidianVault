#include "shared.h"
#include <sys/wait.h>

// Tester for car (Testing SIGINT shm cleanup)

#define DELAY 50000 // 50ms
#define MILLISECOND 1000 // 1ms

pid_t car(const char *, const char *, const char *, const char *);

int main()
{
  shm_unlink("/cab403_car_Test"); // Remove shm object if it exists

  pid_t p;

  // Testing with car launched before server
  p = car("Test", "1", "4", "10");

  // Wait 50ms
  usleep(DELAY);

  // Send SIGINT (simulating ctrl+c)
  kill(p, SIGINT);
  waitpid(p, NULL, 0);

  msg("shm_open(): No such file or directory");
  int shm_fd = shm_open("/cab403_car_Test", O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open()");
  } else {
    fprintf(stderr, "Shared memory object still exists after SIGINT.\n");
    close(shm_fd);
    shm_unlink("/cab403_car_Test");
    return 1;
  }
  printf("\nTests completed.\n");
}


pid_t car(const char *name, const char *lowest_floor, const char *highest_floor, const char *delay)
{
  pid_t pid = fork();
  if (pid == 0) {
    execlp("./car", "./car", name, delay, lowest_floor, highest_floor, NULL);
  }

  return pid;
}
