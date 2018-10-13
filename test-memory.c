#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>

static int alarmed = 1;
#define INTERVAL 1
#define XOR_OPERAND 0xAAAAAAAAAAAAAAAA

static void on_alarm(int signal)
{
    alarmed++;
}

static void set_percent(int index, int percent)
{
    printf("\r\33[%03dC%d%% ", (index * 6), percent);
    fflush(stdout);
}

static void set_error(int index, const char* error)
{
    printf("\33[%dB\r\33[K[%d] %s\33[%dA", index + 1, index, error, index + 1);
    fflush(stdout);
}


static void scrub(int index, size_t len)
{
    void* buffer;
    while(1) {
        buffer = mmap(NULL,
                      len,
                      PROT_READ|PROT_WRITE,
                      MAP_ANON|MAP_PRIVATE,
                      -1,
                      0);
        if(buffer == MAP_FAILED) {
            len -= 4096;
            if(len < 4096) {
                fprintf(stderr, "Not enough memory\n");
                exit(1);
            }
            printf("Failed, retrying with %luGB\n", len / 1024 / 1024 / 1024);
        } else {
            break;
        }
    }

    set_error(index, "Filling buffer");
    uint64_t* ptr = buffer;
    uint64_t* end = (uint64_t*)((char*)buffer + len);
    size_t count = len / sizeof(uint64_t);
    for(ptr = buffer; ptr < end; ptr++) {
        *ptr = ((uint64_t)ptr) ^ XOR_OPERAND;

        if(alarmed) {
            double percent = 
                (((double)((unsigned char*)ptr - (unsigned char*)buffer)) / (double)len) * 100;
            set_percent(index, (int)percent);
            alarmed = 0;
            alarm(INTERVAL);
        }
    }

    set_error(index, "Verifying");
    for(ptr = buffer; ptr < end; ptr++) {
        if(*ptr != (((uint64_t)ptr) ^ XOR_OPERAND)) {
            char err_msg[1024];
            snprintf(err_msg, sizeof(err_msg), "Error found at %p", ptr);
            exit(1);
        }
        if(alarmed) {
            double percent = 
                (((double)((unsigned char*)ptr - (unsigned char*)buffer)) / (double)len) * 100;
            set_percent(index, (int)percent);
            alarmed = 0;
            alarm(INTERVAL);
        }
    }
    set_percent(index, 100);
}

static void usage(const char* name)
{
    fprintf(stderr, "Usage: %s -t <threads> -g <gigabytes_to_allocate>\n",
            name);
}

int main(int argc, char** argv)
{
    const char* gigs_flag = NULL;
    const char* threads_flag = NULL;
    int ch;
    while((ch = getopt(argc, argv, "hg:t:")) != -1) {
        switch(ch) {
            case 'h':
                usage(argv[0]);
                break;
            case 'g':
                gigs_flag = optarg;
                break;
            case 't':
                threads_flag = optarg;
                break;
            default:
                usage(argv[0]);
                exit(1);
        }
    }
    if(!gigs_flag || !threads_flag) {
        usage(argv[0]);
        exit(1);
    }
        
    size_t gigs = atoi(gigs_flag);
    size_t total = gigs * 1024*1024*1024;
    int threads = atoi(threads_flag);
    uint64_t* buffer;
    pid_t children[32];

    for(int i = 0; i < threads; i++) {
        children[i] = fork();
        if(!children[i]) {
            struct sigaction sa = {0};
            sa.sa_handler = on_alarm;
            sigaction(SIGALRM, &sa, NULL);

            size_t size = total / threads;
            scrub(i, size);
            exit(0);
        }
    }

    for(int i = 0; i < threads; i++) {
        int stat_loc;
        waitpid(children[i], &stat_loc, 0);
    }
    for(int i = 0; i < threads; i++)
        printf("\n");

    return 0;
}

