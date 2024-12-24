#include "kernel/types.h"
#include "user/user.h"
void
sieve(int *p1)
{
    int p2[2], n;
    close(p1[1]);    //close father's input
    if(read(p1[0], &n, 1) == 0) exit(0);
    pipe(p2);

    if(fork() == 0){
        sieve(p2);
    }else{
        close(p2[0]);
        printf("prime %d\n", n);
        int prime = n;
        while(read(p1[0], &n, 1)){
            if(n % prime){
                write(p2[1], &n, 1);
            }
        }
        close(p1[0]);
        close(p2[1]);
        wait((int *) 0);
        exit(0);
    }
}

int
main(int argc, char *argv[])
{
    int p1[2], pid;
    pipe(p1);
    pid = fork();
    if(pid < 0){
        printf("fork error\n");
    }else if(pid == 0){
        sieve(p1);
    }else{
        close(p1[0]);
        for(int i=2; i<=35; ++i){
            write(p1[1], &i, 1);
        }
        close(p1[1]);
        wait((int *) 0);
    }
    exit(0);
}

