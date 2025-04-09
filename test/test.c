#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
  int *x = malloc(12);
  memmove(((char*)x) - 4, ((char*)x) - 8, 8);

  free(x+1);

  return 0;
}