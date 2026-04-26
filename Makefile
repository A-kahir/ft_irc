CPP = c++
CPPFLAGS = -Wall -Wextra -Werror -fsanitize=address -g -std=c++98

NAME = ircserv

SRC = srcs/main.cpp \
      srcs/client/client.cpp \
      srcs/client/ft_utils.cpp \
      srcs/parsing/ft_parsing.cpp \
      srcs/parsing/ft_parse_input_cmds.cpp \
      srcs/parsing/ft_parse_input_operators.cpp \
      srcs/parsing/ft_helper_func.cpp \
      srcs/server/ft_first_cmds.cpp \
      srcs/server/ft_parse_operators.cpp \
      srcs/server/Server.cpp \
      srcs/channels/channels.cpp

OBJ = $(SRC:.cpp=.o)

HEADERS = includes/Client.hpp includes/Server.hpp includes/Channels.hpp

all: $(NAME)

$(NAME): $(OBJ)
	$(CPP) $(CPPFLAGS) $(OBJ) -o $(NAME)

%.o: %.cpp $(HEADERS)
	$(CPP) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: clean