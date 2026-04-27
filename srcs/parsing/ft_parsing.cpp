#include "../../includes/Client.hpp"

int ft_parse_args(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Invalid Args" << std::endl;
        return (1);
    }

    if (atoi(argv[1]) < 1024 || atoi(argv[1]) >= 65536)
    {
        std::cerr << "Invalid Port : Make sure you put a correct Port" << std::endl;
        return (1);
    }

    for (size_t i = 0; i < strlen(argv[1]); i++)
    {
        if (argv[1][i] && !isdigit(argv[1][i]))
        {
            std::cerr << "Invalid Args" << std::endl;
            return (1);
        }
    }

    for (size_t i = 0; i < strlen(argv[2]); i++)
    {
        if (argv[2][i] == ' ')
        {
            std::cerr << "Invalid Args" << std::endl;
            return (1);
        }
    }
    return (0);
}

