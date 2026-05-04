#include "../../includes/Client.hpp"
#include "../../includes/Server.hpp"
#include "../../includes/Channels.hpp"

int Server::ft_parse_quit(std::string input, int fd)
{
    std::string reason;
    std::stringstream split(input);
    if (!input.empty())
    {
        while(split >> reason)
        {
            if (reason[0] == ':')
            {
                reason.erase(0, 1);
                std::string rest;
                std::getline(split, rest);
                reason += rest;
                break;
            }
        }
    }
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
                std::string msg;
                if (!reason.empty())
                    msg = ft_build_message(fd) + " QUIT :" + reason + "\r\n";
                else
                     msg = ft_build_message(fd) + " QUIT" + "\r\n";
                send(members[i], msg.c_str(), msg.size(), 0);
            }
        }
    }
    this->RemoveClient(fd);
    return (1);
}
