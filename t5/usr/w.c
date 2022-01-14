#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <lib.h>

int worst_fit(int alg) {
  message m;
  m.m1_i1 = alg;
  return _syscall(MM, WORST_FIT, &m);
}

int main(int argc, char** argv) {
  int result;

  if (argc < 2) return 1;

  result = worst_fit(atoi(argv[1]));
  
  if (result < 0) {
	perror("worst_fit: ");
	return 1;
  }

  return 0;
}
