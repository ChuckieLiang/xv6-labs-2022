#include "kernel/types.h"
// #include "kernel/stat.h"
#include "user/user.h"
int
main(int argc, char *argv[])
{
    int pleft[2], pright[2], pid;
    char buf[2];
    pipe(pleft);
    pipe(pright);
    pid = fork();
    if(pid < 0){
        printf("fork error\n");
    }else if(pid > 0){
        close(pright[0]);
        close(pleft[1]);
        write(pright[1],"\n",1);
        int ret = read(pleft[0],buf,1);
        buf[ret] = 0;
        printf("%d: received pong\n",getpid());
    }else{
        close(pright[1]);
        close(pleft[0]);
        int ret = read(pright[0],buf,1);
        buf[ret] = 0;
        printf("%d: received ping\n",getpid());
        write(pleft[0],"\n",1);
    }
    exit(0);
}