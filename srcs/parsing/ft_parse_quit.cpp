#include "../../includes/Server.hpp"

int ft_parse_quit(std::string input, int fd)
{
    if (input.empty())
        return (ft_message(fd, "461", " :Not enough parameters", ""), 1);
    
}