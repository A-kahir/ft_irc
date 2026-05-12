#include "../../includes/Channels.hpp"
#include "../../includes/Client.hpp"

channels::channels() : topic("No topic is set")
{
    this->invit_only = 0;
    this->topic_rest = 0;
    user_limit = -1;
    this->pswrd = "";
}

std::vector<int> channels::getMembers()
{
    std::vector<int> fds;
    for (size_t i = 0; i < this->users.size(); i++)
        fds.push_back(this->users[i]->get_fd());
    return fds;
}

bool channels::isMember(int fd)
{
    for (size_t i = 0; i < this->users.size(); i++)
    {
        if (this->users[i]->get_fd() == fd)
            return true;
    }
    return false;
}
