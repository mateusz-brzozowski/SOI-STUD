#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lib.h>

int set_p_group(unsigned int p_group)
{
    message m;
    m.m1_i2 = p_group;
    return _syscall(MM, 79, &m);
}

int main(int argc, char* argv[])
{
    int p_id;
    unsigned int p_group, p_group_res;
    if (argc != 2)
    {
        printf("Invalid arguments\n");
        return 1;
    }
    p_group = atoi(argv[1]);
    p_group_res = set_p_group(p_group);
    printf("Start group: %d, Result group: %d\n", p_group, p_group_res);
    while(1);
}
