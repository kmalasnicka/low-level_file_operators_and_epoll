/* The program runs a series of small tests and prints how long each one took.
 * Your job is to implement the functions marked with STAGE1, STAGE2 and STAGE3.
 * Each test should return 0 on success and -1 on failure.
 *
 * STAGE1: write/read with low-level POSIX I/O (open, write, read, close).
 * STAGE2: the same with C streams (fwrite, fread, fputc, fgetc, fscanf).
 * STAGE3: wait for timer events in three ways: busy waiting, busy waiting with
 *         sleep, and poll.
 *
 * All files are created in a fresh temporary directory under /tmp.
 * Compare the printed times after each stage and try to explain them.
 */

#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#define DATA_SIZE (1024 * 1024 + 3)

/* Return 0 on success and -1 on failure.
 * Write tests create the file which are then readed by read tests
 * block_size is 1 or 4096 for block tests.
 */
int test_write(const char* path, const unsigned char* data, size_t size, size_t block_size)
{
    //write - writes btes from a buffer to a file, returns number of bytes written or -1 on error
    /* STAGE1: create the file and write size bytes of data with write(), block_size bytes at a time.
     * size does not have to be divisible by block_size, so the last block can be shorter. */
    double start = now(CLOCK_MONOTONIC);
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if(fd == -1){
        perror("open");
        return -1;
    }
    ssize_t written = 0;
    int result = -1;
    while(written < size){ //dopoki nie zapisalismy wszystkiego (size)
        size_t remaining = size - written;
        size_t to_write = remaining < block_size ? remaining : block_size;
        ssize_t w = write(fd, data + written, to_write);
        if(w <= 0) {
            perror("write");
            close(fd);
            return -1;
        }
        written+=w;
    }
    if(close(fd) == -1) {
        perror("close");
        return -1;
    }
    result = 0;
    if (result == 0)
        printf("  write block=%-4zu %9.3f ms  OK\n", block_size, elapsed(CLOCK_MONOTONIC, start) * 1000);
    return result;
}

int test_read(const char* path, size_t size, size_t block_size)
{
    //Reads bytes from a file into a buffer. Returns bytes read, 0 at EOF, or -1 on error.
    double start = now(CLOCK_MONOTONIC);
    /* STAGE1: read the file with read(), block_size bytes at a time, until end of file. */
    int fd = open(path, O_RDONLY);
    if(fd == -1){
        perror("open");
        return -1;
    }  
    ssize_t n;
    ssize_t r = 0;
    char buf[block_size];
    int result = -1;
    while(1){
        n = read(fd, buf, block_size);
        if(n == -1){
            perror("read");
            return -1;
        }
        if(n == 0){
            break; //EOF
        }
        r+=n;
    }
    if(close(fd) == -1){
        perror("close");
        return -1;
    }
    if(r != size){
        fprintf(stderr, "incorrect file size\n");
        return -1;
    }
    result = 0;
    if (result == 0)
        printf("  read block=%-4zu %9.3f ms  OK\n", block_size, elapsed(CLOCK_MONOTONIC, start) * 1000);
    return result;
}

int test_fwrite(const char* path, const unsigned char* data, size_t size, size_t block_size)
{
    //Writes multiple elements from a buffer to a file.
    double start = now(CLOCK_MONOTONIC);
    /* STAGE2: like test_write, but with fwrite(). */
    FILE *file = fopen(path, "w");
    if(file == NULL) {
        perror("fopen");
        return -1;
    }
    int result = -1;
    size_t written = 0;
    while(written < size){
        size_t remaining = size - written;
        size_t to_write = remaining < block_size ? remaining : block_size;
        size_t w = fwrite(data + written, 1, to_write, file); //kazdy zapisywany element ma 1 bajt
        if(w != to_write){
            perror("fwrite");
            fclose(file);
            return -1;
        }
        written+=w;
    }

    if(fclose(file) != 0){
        perror("fclose");
        return -1;
    }
    result = 0;
    if (result == 0)
        printf("  fwrite block=%-4zu %9.3f ms  OK\n", block_size, elapsed(CLOCK_MONOTONIC, start) * 1000);
    return result;
}

int test_fread(const char* path, size_t size, size_t block_size)
{
    //Reads multiple elements from a file into a buffer.
    double start = now(CLOCK_MONOTONIC);
    /* STAGE2: like test_read, but with fread(). */
    FILE *file = fopen(path, "r");
    if(file == NULL) {
        perror("fopen");
        return -1;
    }
    int result = -1;
    size_t n;
    char buf[block_size];
    while(1){
        n = fread(buf, 1, block_size, file);
        if(n < block_size){
            if(ferror(file)){
                perror("fread");
                fclose(file);
                return -1;
            } else if(feof(file)){
                break;
            }
        }
    }
    if(fclose(file) != 0){
        perror("fclose");
        return -1;
    }
    result = 0;
    if (result == 0)
        printf("  fread block=%-4zu %9.3f ms  OK\n", block_size, elapsed(CLOCK_MONOTONIC, start) * 1000);
    return result;
}

int test_fputc(const char* path, const unsigned char* data, size_t size)
{
    //Writes one character (byte) to a file.
    double start = now(CLOCK_MONOTONIC);
    /* STAGE2: create the file and write size bytes of data with fputc(), one character at a time. */
    FILE *file = fopen(path, "w");
    if(file == NULL) {
        perror("fopen");
        return -1;
    }
    int result = -1;
    for(size_t i = 0; i < size; i++){
        char c = data[i];
        int c_putc = fputc(c, file);
        if(c_putc == EOF){
            perror("fputc");
            fclose(file);
            return -1;
        }
    }
    if(fclose(file) != 0){
        perror("fclose");
        return -1;
    }
    result = 0;
    if (result == 0)
        printf("  fputc           %9.3f ms  OK\n", elapsed(CLOCK_MONOTONIC, start) * 1000);
    return result;
}

int test_fgetc(const char* path, size_t size)
{
    //Reads one character (byte) from a file.
    double start = now(CLOCK_MONOTONIC);
    /* STAGE2: read the file with fgetc(), one character at a time, until end of file. */
    FILE *file = fopen(path, "r");
    if(file == NULL) {
        perror("fopen");
        return -1;
    }
    int result = -1;
    while(1){
        int n = fgetc(file);
        if(n == EOF){ //koniec pliku lub blad 
            if(ferror(file)){
                perror("fgetc");
                fclose(file);
                return -1;
            }
            break;
        }
    }
    if(fclose(file) != 0){
        perror("fclose");
        return -1;
    }
    result = 0;
    if (result == 0)
        printf("  fgetc           %9.3f ms  OK\n", elapsed(CLOCK_MONOTONIC, start) * 1000);
    return result;
}

int test_fscanf(const char* path, size_t size)
{
    //Reads formatted input from a file, e.g. %c for one character.
    double start = now(CLOCK_MONOTONIC);
    /* STAGE2: read the file with fscanf(), one character at a time, until end of file. */
    FILE *file = fopen(path, "r");
    if(file == NULL) {
        perror("fopen");
        return -1;
    }
    int result = -1;
    char character;
    while(1){
        int n = fscanf(file, "%c", &character);
        if(n == EOF){
            if(ferror(file)){
                perror("fscanf");
                fclose(file);
                return -1;
            }
            break;
        }
        if(n != 1){
            fprintf(stderr, "fscanf failed\n");
            fclose(file);
            return -1;
        }
    }
    if(fclose(file) != 0){
        perror("fclose");
        return -1;
    }
    result = 0;
    if (result == 0)
        printf("  fscanf           %9.3f ms  OK\n", elapsed(CLOCK_MONOTONIC, start) * 1000);
    return result;
}

enum wait_mode
{
    WAIT_BUSY,
    WAIT_SLEEP,
    WAIT_POLL
};

/// Return 0 on success, -1 on error.
int test_wait(enum wait_mode mode)
{
    // initialization - boilerplate
    const char* name;
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK); //creates a timer and returns a file descriptor, TFD_NONBLOCK makes read nonblocking
    if (fd == -1)
    {
        perror("timerfd_create");
        return -1;
    }
    struct itimerspec timer = {.it_interval = {0, INTERVAL_NS}};
    if (clock_gettime(CLOCK_MONOTONIC, &timer.it_value) == -1)
    {
        perror("clock_gettime");
        close(fd);
        return -1;
    }
    timer.it_value.tv_nsec += INTERVAL_NS;
    if (timer.it_value.tv_nsec >= 1000000000L)
    {
        timer.it_value.tv_sec++;
        timer.it_value.tv_nsec -= 1000000000L;
    }
    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer, NULL) == -1)
    {
        perror("timerfd_settime");
        close(fd);
        return -1;
    }
    struct timer_stats stats = {.first_deadline = timer.it_value.tv_sec + timer.it_value.tv_nsec / 1e9};
    double wall = now(CLOCK_MONOTONIC);
    double cpu = now(CLOCK_PROCESS_CPUTIME_ID);

    int result = -1;

    // Real test starts here
    // call `read` and then `record_expirations(&stats, expirations, now(CLOCK_MONOTONIC))`,
    // where `expirations` is read from the timerfd, until stats.count reaches EXPIRATION_COUNT
    switch (mode)
    {
        case WAIT_BUSY: //repeated reads, high cpu usage
        {
            name = "busy";
            // STAGE3: implement busy waiting here
            while(stats.count < EXPIRATION_COUNT){ //timer wygasa wielokrotnie, a zadanie wymaga zebrania EXPIRATION_COUNT wygaśnięć
                uint64_t expirations;
                ssize_t n = read(fd, &expirations, sizeof(expirations));
                if(n == -1 && errno == EAGAIN){
                    continue;
                } else if(n == -1) {
                    perror("read");
                    close(fd);
                    return -1;
                }
                if(n != sizeof(expirations)){
                    fprintf(stderr, "Invalid timer read\n");
                    close(fd);
                    return -1;
                }
                record_expirations(&stats, expirations, now(CLOCK_MONOTONIC));
            }
            result = 0;
            break;
        }
        case WAIT_SLEEP: //read + sleep, lower cpu usage
        {
            name = "sleep";
            // STAGE3: implement busy waiting with sleep here
            while(stats.count < EXPIRATION_COUNT){
                uint64_t expirations;
                ssize_t n = read(fd, &expirations, sizeof(expirations));
                if (n == -1 && errno == EAGAIN) {
                    usleep(1000);  // sleep for 1 millisecond
                    continue;
                } else if(n == -1) {
                    perror("read");
                    close(fd);
                    return -1;
                }
                if(n != sizeof(expirations)){
                    fprintf(stderr, "Invalid timer read\n");
                    close(fd);
                    return -1;
                }
                record_expirations(&stats, expirations, now(CLOCK_MONOTONIC));
            }
            result = 0;
            break;
        }
         case WAIT_POLL: //wykonuje poll(), który czeka na zdarzenie, i dopiero wtedy read().
        {
        name = "poll";
        // STAGE3: implement waiting with poll here
        //struktura pollfd, trzymajaca deskryptor, zdarzenia na ktore chcemy czekac i zdarzenia ktore wystapily
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN; //POLLIN czyli odczyt danych
        while(stats.count < EXPIRATION_COUNT){
            int ready = poll(&pfd, 1, -1); //wywolujemy poll, 1 - l obserwowanych desk, -1 nieskonczony timeout 
            if(ready < 0){ //blad
                if(errno == EINTR) continue; //poll zostalo przerwane sygnalem 
                perror("poll");
                close(fd);
                return -1;
            } 
            if(ready == 0){ //timeout
                continue;
            }
            //co najmniej jeden deskryptor zgłosił zdarzenie.
            if(pfd.revents & POLLIN){ 
//sprawdzamy czy w revents ustawiony jest POLLIN, czyli ze deskryptor jest gotowy do odczytu
//odczytujemy wygasniecie timera         
                uint64_t expirations;
                ssize_t n = read(fd, &expirations, sizeof(expirations));
                if (n == -1 && errno == EAGAIN) continue;

                if (n != sizeof(expirations)) {
                    if (n == -1)
                        perror("read");
                    else
                        fprintf(stderr, "Invalid timer read\n");

                    close(fd);
                    return -1;
                }
                //aktualizacja statystyk, liczby wygasniec i opoznienia
                record_expirations(&stats, expirations, now(CLOCK_MONOTONIC));
            } else {
                fprintf(stderr, "Unexpected poll event\n");
                close(fd);
                return -1;
            }
        }

        result = 0;
        break;
    }
        default:
            return -1;
    }

    if (close(fd) == -1 || (result == 0 && stats.count != EXPIRATION_COUNT))
        result = -1;
    if (result == 0)
        printf(
            "  %s wall=%7.1f ms  CPU=%7.1f ms  expirations=%-2u  successful_reads=%-2u  avg delay=%6.2f ms  max "
            "delay=%6.2f ms  OK\n",
            name, elapsed(CLOCK_MONOTONIC, wall) * 1000, elapsed(CLOCK_PROCESS_CPUTIME_ID, cpu) * 1000, stats.count,
            stats.reads, stats.total_delay / stats.count * 1000, stats.max_delay * 1000);
    return result;
}

int main(void)
{
    static unsigned char data[DATA_SIZE];
    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = (unsigned char)(i * 37 + i / 256);

    char directory[] = "/tmp/sop1-workshop-XXXXXX";
    if (!mkdtemp(directory))
    {
        perror("mkdtemp");
        return EXIT_FAILURE;
    }
    printf("Temporary directory: %s\n", directory);
    chdir(directory);

    int failed = 0;
    puts("POSIX: WRITE AND READ");
    if (report_result("write block=1", test_write("write_1", data, sizeof(data), 1)))
        failed = 1;
    if (report_result("read block=1", test_read("write_1", sizeof(data), 1)))
        failed = 1;
    if (report_result("write block=4096", test_write("write_4096", data, sizeof(data), 4096)))
        failed = 1;
    if (report_result("read block=4096", test_read("write_4096", sizeof(data), 4096)))
        failed = 1;

    puts("C STREAMS: FWRITE AND FREAD");
    if (report_result("fwrite block=1", test_fwrite("fwrite_1", data, sizeof(data), 1)))
        failed = 1;
    if (report_result("fread block=1", test_fread("fwrite_1", sizeof(data), 1)))
        failed = 1;
    if (report_result("fwrite block=4096", test_fwrite("fwrite_4096", data, sizeof(data), 4096)))
        failed = 1;
    if (report_result("fread block=4096", test_fread("fwrite_4096", sizeof(data), 4096)))
        failed = 1;

    puts("C STREAMS: CHARACTER AND FORMATTED I/O");
    if (report_result("fputc", test_fputc("fputc_path", data, sizeof(data))))
        failed = 1;
    if (report_result("fgetc", test_fgetc("fputc_path", sizeof(data))))
        failed = 1;
    if (report_result("fscanf", test_fscanf("fputc_path", sizeof(data))))
        failed = 1;

    puts("WAITING FOR EVENTS");
    if (report_result("busy", test_wait(WAIT_BUSY)))
        failed = 1;
    if (report_result("sleep", test_wait(WAIT_SLEEP)))
        failed = 1;
    if (report_result("poll", test_wait(WAIT_POLL)))
        failed = 1;

    if (remove_test_file("write_1") == -1)
        failed = 1;
    if (remove_test_file("write_4096") == -1)
        failed = 1;
    if (remove_test_file("fwrite_1") == -1)
        failed = 1;
    if (remove_test_file("fwrite_4096") == -1)
        failed = 1;
    if (remove_test_file("fputc_path") == -1)
        failed = 1;
    if (rmdir(directory) == -1)
    {
        perror("rmdir");
        failed = 1;
    }
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
