#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int ppl[2], ppr[2];
  int pid;
  int i, first, n;

  pipe(ppl);

  pid = fork();

  while(pid == 0){
    close(ppl[1]);
    n = read(ppl[0], &first, 4);
    if(n<=0) exit(0);
    printf("prime %d\n", first);
    pipe(ppr);
    
    pid = fork();
    if(pid==0){
      ppl[0] = ppr[0];
      ppl[1] = ppr[1];
      continue;
    }
    close(ppr[0]);
    while(read(ppl[0], &i, 4)){
      if(i%first!=0)
        write(ppr[1], &i, 4);
    }
    close(ppr[1]);
    close(ppl[0]);
    wait(0);
    exit(0);
  }

  close(ppl[0]);
  for(i=2;i<=35;i++)
    write(ppl[1], &i, 4);
  close(ppl[1]);
  wait(0);

  exit(0);
}
