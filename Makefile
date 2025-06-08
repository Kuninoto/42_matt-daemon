CC = c++
CFLAGS = -Wall -Wextra -Werror -std=c++20 # -D _DEBUG=1 -g -fsanitize=address
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

run: re
	$(info Running $(NAME)...)
	sudo ./$(NAME)

noleaks: re
	sudo valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes -s ./$(NAME) 

fmt:
	clang-format -i src/*.cpp src/*.hpp

.PHONY: all $(NAME) $(OBJ_DIR) clean fclean re fmt run

.SILENT:
