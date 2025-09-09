#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
  const char* freezer_on = getenv("FREEZER_ON");
  if (freezer_on != NULL && freezer_on[0] == '1') {
    printf("Freezer on\n");
  } else {
    printf("Freezer off\n");
  }

  return 0;
}