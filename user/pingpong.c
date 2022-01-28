#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int pp1[2], pp2[2];
  char msg;
  int pid;

  pipe(pp1);
  pipe(pp2);
  
  pid = fork();
  if(pid == 0){
    pid = getpid();
    close(pp1[1]);
    close(pp2[0]);
    read(pp1[0], &msg, 1);
    printf("%d: received ping\n", pid);
    write(pp2[1], &msg, 1);
  }
  else{
    pid = getpid();
    close(pp1[0]);
    close(pp2[1]);
    write(pp1[1], &msg, 1);
    read(pp2[0], &msg, 1);
    printf("%d: received pong\n", pid);
  }

  exit(0);
}
