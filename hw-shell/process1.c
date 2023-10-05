#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    printf("Welcome ");
    printf("process1: pid = %d, pgid = %d\n", getpid(), getpgrp());
    return 0;
}