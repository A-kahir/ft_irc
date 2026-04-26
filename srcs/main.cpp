#include "../includes/Client.hpp"
#include "../includes/Channels.hpp"
#include "../includes/Server.hpp"

int main(int argc, char **argv)
{
    ft_parse_args(argc, argv);
    Server server(atoi(argv[1]), argv[2]);
    server.Start();
    close (server.get_fd());
}
