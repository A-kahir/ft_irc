#ifndef CHANNELS_HPP
#define CHANNELS_HPP

#include <string>
#include <vector>

class client;
class Server;

class channels
{
    public:
        std::string topic;
        std::string name;
        std::vector<client*> users;
        std::vector<client*> operators;

        channels();
        bool invit_only;
        bool topic_rest;
        int user_limit;
        std::string pswrd;
};

#endif