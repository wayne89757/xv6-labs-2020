#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: xargs prog ...\n");
    exit(1);
  }

  char buf[512], ch, *args[MAXARG];
  int i;

  for(i=1; i<argc; i++) args[i-1] = argv[i];
  
  i = 0;
  while(read(0, &ch, 1) > 0){
    if(ch=='\n'){
      buf[i] = 0;
      args[argc-1] = buf;
      args[argc] = 0;

      if(fork() == 0){
        exec(args[0], args);
      }
      else{
        wait(0);
      }

      i = 0;
    }
    else{
      buf[i++] = ch;
    }
  }

  exit(0);
}
