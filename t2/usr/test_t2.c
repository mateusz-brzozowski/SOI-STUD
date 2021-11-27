#include <stdio.h>
#include <lib.h>

unsigned int set_p_group(unsigned int p_group)
{
    message m;
    m.m1_i1 = p_group;
    return _syscall(MM, SET_P_GROUP, &m);
}

unsigned int get_p_group(void) 
{
    message m;
    return _syscall(MM, GET_P_GROUP, &m);
}

int main(int argc, char* argv[])
{
    int res;
    unsigned int p_group;
    unsigned int p_group_after_set;
    if (argc != 2)
    {
        printf("Invalid arguments\n");
    }
    p_group = atoi(argv[1]);
    set_p_group(p_group);
    p_group_after_set = get_p_group();
    printf("Dest: %d\n", p_group);
    printf("Group: %d\n", p_group_after_set);
    while(1);
    return 0;
}