#include <stdio.h>
#include <lib.h>

int getprocnr(int pid)
{
    message m;
    m.m1_i1 = pid;
    return (_syscall(MM, 78, &m));
}

int main(int argc, char* argv[])
{
    int p_id = atoi(argv[1]);   /* proccess id (cast string to int)*/
    int i;
    int t_id;

    if(argc < 2)
    {
        printf("Missing argument\n");
        return 1;
    }

    for(i = p_id; i >= p_id - 10; --i)
    {
        t_id = getprocnr(i);  /* retrun number of proccess in table */
        if(t_id >= 0)
        {
            printf("Proccess id: %d; Index in table: %d\n", i, t_id);
        }
        else
        {
            printf("Missing proccess: %d; Error: %d\n", i, errno);
        }
    }
    return 0;
}
