#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int ppl[2], ppr[2];
  int pid, first, i;

  pipe(ppr);
  
  pid = fork();
  if(pid==0){
     while(pid==0){
      ppl[0] = ppr[0];
      close(ppr[1]);
      if(read(ppl[0], &first, 4)==0) break;
      printf("prime %d\n", first);
      
      pipe(ppr);
      pid = fork();
      if(pid == 0){
        continue;
      }
      else{
        while(read(ppl[0], &i, 4))
          if(i%first!=0)
            write(ppr[1], &i, 4);

        close(ppr[1]);
        close(ppl[0]);
        wait(0);
      }
     }
  }
  else{
    close(ppr[0]);
    for(int i=2;i<=35;i++)
      write(ppr[1], &i, 4);
    close(ppr[1]);
    wait(0);
  }

  exit(0);
}
