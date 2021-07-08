cmake_minimum_required(VERSION 3.1)
project(ft_traceroute C)

set(CMAKE_C_STANDARD 99)

add_executable(ft_traceroute main.c traceroute.h initialize_context.c send_icmp_msg_v4.c send_udp_trcrt_msg_v4.c)