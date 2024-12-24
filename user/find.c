#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
void find(char *path, char *target);
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        exit(0);
    }
    char *scope = argv[1];
    char *target = argv[2];
    find(scope, target);
    exit(0);
}

char *
fmtname(char *path)
{
    char *p;
    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;
    return p;
}
void find(char *path, char *target)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    if ((fd = open(path, 0)) < 0)
    {
        fprintf(1, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, &st) < 0)
    {
        fprintf(1, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_DEVICE:
    case T_FILE:
        if (strcmp(fmtname(path), target) == 0)
        {
            printf("%s\n", path);
        }
        // else{
        //     printf("strcmp(%s, %s) = %d\n", fmtname(path), target,strcmp(fmtname(path), target));
        // }
        break;
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            {
                continue;
            }
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            // printf("%s  fmtname = %s\n",buf,fmtname(buf));
            find(buf, target);
        }
        break;
    }
    close(fd);
}