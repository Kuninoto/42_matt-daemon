CC = c++
CFLAGS = -Wall -Wextra -Werror -std=c++20 -g -D _DEBUG=1 # -fsanitize=address
RM = rm -rf

NAME = MattDaemon

SRCS = Client.cpp Server.cpp signal.cpp Tintin_reporter.cpp main.cpp

OBJ_DIR = obj
OBJS = $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

all: $(NAME)

$(NAME): $(OBJ_DIR) $(OBJS)
	$(info Linking $(NAME)...)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)
	$(info Done!)

$(OBJ_DIR):
	mkdir -p obj

$(OBJ_DIR)/%.o: src/%.cpp
	$(info Compiling $<...)
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

fmt:
	clang-format -i src/*.cpp src/*.hpp

run: re
	sudo ./$(NAME)

noleaks: re
	sudo valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes -s ./$(NAME) 

.PHONY: all $(NAME) $(OBJ_DIR) clean fclean re fmt run

.SILENT:
