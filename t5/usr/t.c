#include <stdio.h>
#include <unistd.h>
#include <lib.h>

int hole_map(void* buffer, size_t nbytes) {
  message m;
  m.m1_i1 = nbytes;
  m.m1_p1 = buffer;
  return _syscall(MM, HOLE_MAP, &m);
}

int main(void) {
  unsigned int buffer[1024];
  unsigned int i;
  int result;

  result = hole_map(buffer, sizeof(buffer));
  if (result < 0) {
	perror("hole_map: ");
	return 1;
  }

  printf("[%d]", result);
  for (i = 0; i < 1024 && buffer[i]; i += 2) printf("\t%d", buffer[i]);
  putc('\n', stdout);

  return 0;
}
