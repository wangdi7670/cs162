#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int buff_size = 64;
    char buff[buff_size];

    while ((read(STDIN_FILENO, buff, buff_size)) > 0) {
        printf("%s", buff);
    }
    printf("to ");

    return 0;
}