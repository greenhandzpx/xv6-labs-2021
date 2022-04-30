#include "kernel/types.h"
#include "user/user.h"

int main() 
{
    int fds1[2];
    int fds2[2];
    pipe(fds1);
    pipe(fds2);
    int pid = fork();
    if (pid == 0) {
        char buf;
        int id = getpid();
        read(fds1[0], &buf, 1);
        printf("%d: received ping\n", id);
        write(fds2[1], &buf, 1);
        exit(0);
    } else {
        char buf = 'o';
        int id = getpid();
        write(fds1[1], &buf, 1);
        read(fds2[0], &buf, 1);
        printf("%d: received pong\n", id);
        exit(0);
    }
    exit(0);
}