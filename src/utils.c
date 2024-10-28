#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "./include/utils.h"

void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);   // 清空屏幕
    write(STDOUT_FILENO, "\x1b[1;1H", 6); // 设置光标位置为左上角
    perror(s);
    exit(1);
}
