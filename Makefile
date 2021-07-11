# **************************************************************************** #
# GENERIC_VARIABLES

OBJ_DIR = build

# **************************************************************************** #
# COMPILER_OPTIONS

C_COMPILER = clang
C_STANDART = 
C_CFLAGS =  $(CFLAGS) $(CPPFLAGS) -Wall -Wextra -Werror
C_LFLAGS =  $(CFLAGS) $(CPPFLAGS) -Wall -Wextra -Werror

# **************************************************************************** #
# FT_TRACEROUTE TARGET DESCRIPTION

FT_TRACEROUTE_NAME = ft_traceroute
FT_TRACEROUTE_PATH = .
FT_TRACEROUTE_FILE = build/ft_traceroute
FT_TRACEROUTE_SRCS = dns_resolvers.c initialize_context.c main.c process_trace.c send_icmp_msg_v4.c send_udp_trcrt_msg_v4.c
FT_TRACEROUTE_OBJS = $(patsubst %, $(OBJ_DIR)/%.o, $(FT_TRACEROUTE_SRCS))
FT_TRACEROUTE_DEPS = $(patsubst %, $(OBJ_DIR)/%.d, $(FT_TRACEROUTE_SRCS))
FT_TRACEROUTE_LIBS = 
FT_TRACEROUTE_INCS = 

# **************************************************************************** #
# GENERIC RULES

.PHONY: all re clean fclean
.DEFAULT_GOAL = all

all:  $(FT_TRACEROUTE_FILE)

clean:
	@rm -rf $(OBJ_DIR)

fclean: clean
	@rm -rf  $(FT_TRACEROUTE_FILE)

re: fclean all

$(FT_TRACEROUTE_FILE):  $(FT_TRACEROUTE_OBJS)
	@$(C_COMPILER) $(C_LFLAGS) $(C_STANDART) -o $(FT_TRACEROUTE_FILE) $(FT_TRACEROUTE_OBJS)  $(FT_TRACEROUTE_LIBS)
	@printf 'Finished	\033[1;32m\033[7m$@ \033[0m\n\n'

$(OBJ_DIR)/%.c.o: $(FT_TRACEROUTE_PATH)/%.c
	@mkdir -p $(OBJ_DIR)
	@printf 'Compiling	\033[1;33m$<\033[0m ...\n'
	@$(C_COMPILER) $(C_CFLAGS) $(C_STANDART) $(FT_TRACEROUTE_INCS) -o $@ -c $< -MMD

-include $(FT_TRACEROUTE_DEPS)
