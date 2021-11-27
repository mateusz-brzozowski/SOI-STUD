#include <stdio.h>
#include <lib.h>

int set_p_group(int p_id, unsigned int p_group)
{
    message m;
    m.m1_i1 = p_id;
    m.m1_i2 = p_group;
    return _syscall(MM, 79, &m);
}

int main(int argc, char* argv[])
{
    int res, p_id;
    unsigned int p_group;
    if (argc != 3)
    {
        printf("Invalid arguments\n");
    }
    p_id = atoi(argv[1]);
    p_group = atoi(argv[2]);
    set_p_group(p_id, p_group);
    return 0;
}