#include "../../includes/Server.hpp"
#include "../../includes/Client.hpp"
#include "../../includes/Channels.hpp"

channels* Server::ft_get_channel(std::string name)
{
    for (size_t i = 0; i < this->chans.size(); i++)
    {
        if (this->chans[i].name == name)
            return (&this->chans[i]);
    }
    return (NULL);
}

client* Server::ft_get_user_by_nick(std::string nick)
{
    for (size_t i = 0; i < this->clients.size(); i++)
    {
        if (this->clients[i].get_nick() == nick)
            return (&this->clients[i]);
    }
    return (NULL);
}

bool Server::ft_is_user_in_channel(channels *chan, int fd)
{
    for (size_t i = 0; i < chan->users.size(); i++)
    {
        if (chan->users[i]->get_fd() == fd)
            return (1);
    }
    return (0);
}

std::string Server::ft_build_message(int fd)
{
    return ":" + this->clients[fd].get_nick() + "!" +
           this->clients[fd].get_user() + "@" +
           this->clients[fd].get_hostname();
}

int Server::ft_check_max_chans_joined(int fd)
{
	int counter = 1;
	for (size_t i = 0; i < this->chans.size(); i++)
	{
		if (ft_is_user_in_channel(&this->chans[i], fd) == 1)
			counter++;
	}
	return (counter);
}
