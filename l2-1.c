#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_BUF 256

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s path1 path2", argv[0]);
    exit(EXIT_FAILURE);
}
int main(int argc, char **argv){
    if(argc != 3){
        usage(argc, argv);
    }

    int fd1, fd2;
    char *path1 = argv[1];
    char *path2 = argv[2];

    fd1 = open(path1, O_RDONLY);
    if(fd1 == -1) ERR("open");

    fd2 = open(path2, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if(fd2 == -1) ERR("open");

    char file_buf[MAX_BUF];
    while(1){
        ssize_t read_size = read(fd1, file_buf, MAX_BUF);
        if(read_size == -1) ERR("read");
        if(read_size == 0) break;

        ssize_t written = 0;
        while(written < read_size){
            ssize_t n = write(fd2, file_buf, read_size);
            if(n == -1 || n == 0) ERR("write");
            written += n;
        }
    }

    if(close(fd1) == -1) ERR("close");
    if(close(fd2) == -1) ERR("close");
    return EXIT_SUCCESS;
}
