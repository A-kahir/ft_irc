#include "../../includes/Client.hpp"
#include "../../includes/Server.hpp"
#include "../../includes/Channels.hpp"

int Server::ft_parse_quit(std::string input, int fd)
{
    if (input.empty())
        return (ft_message(fd, "461", " :Not enough parameters", ""), 1);
    std::stringstream split(input);
    std::string reason;
    int flag_reason = 0;
    while(split >> reason)
    {
        if (reason[0] == ':')
        {
            flag_reason = 1;
            reason.erase(0, 1);
            std::string rest;
            std::getline(split, rest);
            reason += rest;
            break;
        }
    }
    if (!flag_reason)
        return (ft_message(fd, "412", " :No reason to quit", ""), 1);
    client &cl = this->clients[fd];
    for (size_t i = 0; i < this->chans.size(); i++)
    {
        channels &chan = this->chans[i];
        if (!chan.isMember(fd))
            continue ;
        std::vector<int> members = chan.getMembers();
        for (size_t i = 0; i < members.size(); i++)
        {
            if (members[i] != fd)
            {
                std::string msg = ":" + cl.get_nick() + "!" + cl.get_user() + " QUIT :" + reason + "\r\n";
                send(members[i], msg.c_str(), msg.size(), 0);
            }
        }
    }
    this->RemoveClient(fd);
    return (1);
}