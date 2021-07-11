#include "traceroute.h"

void process_trace();

/*
 * Program: ft_traceroute
 *
 */

int main(int ac, char **av)
{
    initialize_context(ac, av);
    process_trace();

    return 0;
}
