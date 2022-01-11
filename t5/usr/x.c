#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
  sleep(argc >= 2 ? atoi(argv[1]) : 0);
  return 0;
}
