#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int time;

  if(argc < 2){
    fprintf(2, "Usage: sleep ticks...\n");
    exit(1);
  }

  time = atoi(argv[1]);
  if(time < 0) time = 0;
  if(sleep(time)) exit(1);
  
  exit(0);
}
