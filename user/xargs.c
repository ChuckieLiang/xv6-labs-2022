#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#define MAX_LEN 100

int main(int argc, char *argv[])
{
    if(argc < 2){
        printf("Please enter .. |xargs [command] ...\n");
        exit(1);
    }else if(argc > MAXARG){
        printf("The number of command is too many!\n");
        exit(1);
    }
    char *command = argv[1];
    char *paramv[MAXARG];
    for (int i = 1; i < argc; i++)
    {
        paramv[i - 1] = argv[i];
    }
    paramv[argc-1] = malloc(512);

    while (gets(paramv[argc-1], MAX_LEN))
    {
        if (paramv[argc-1][0] == 0) break;
        for(int i=0; i<MAX_LEN; ++i)
        {
            if(paramv[argc-1][i] == '\n')
            {
                paramv[argc-1][i] = 0;
                break;
            }
        }
        if (fork() == 0)
        {
            exec(command, paramv);
            exit(0);
        }else
        {
            wait((int *) 0);
        }
    }
    exit(0);
}
//sh < xargstest.sh

    // gets(paramv[argc-2], MAX_LEN);
    // printf("paramv[argc-2] = %s",paramv[argc-2]);

    // gets(paramv[argc-2], MAX_LEN);
    // printf("paramv[argc-2] = %s",paramv[argc-2]);

    // gets(paramv[argc-2], MAX_LEN);
    // printf("paramv[argc-2] = %s",paramv[argc-2]);

