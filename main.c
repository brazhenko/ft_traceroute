#include "traceroute.h"

void process_trace();

int main(int ac, char **av)
{
    initialize_context(ac, av);
    process_trace();

    return 0;
}
