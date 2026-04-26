#include "../../includes/Client.hpp"

client::client()
{
    Auth_pass = 0;
    Auth_nick = 0;
    Auth_user = 0;
    Auth_done = 0;
    prv_count = 0;
    fd = -1;
    connect_t = std::time(NULL);
    flag_invitation = 0;
}

client::~client()
{}

bool client::Auth_is_registered()
{
    if (Auth_pass && Auth_nick && Auth_user)
        return (1);
    return (0);
}

void client::appendToBuffer(const std::string data)
{
    this->buffer += data;
}

std::string& client::get_buffer()
{
    return this->buffer;
}

int client::get_fd() const
{
    return (fd);
}

void client::set_fd(int new_fd)
{
    fd = new_fd;
}

std::string client::get_nick() const
{
    return (nick);
}

void client::set_nick(std::string input)
{
    nick = input;
}

std::string client::get_user() const
{
    return (user);
}

void client::set_user(std::string input)
{
    user = input;
}

std::string client::get_hostname() const
{
    return (hostname);
}

void client::set_hostname(std::string input)
{
    hostname = input;
}

std::string client::get_servername() const
{
    return (servername);
}

void client::set_servername(std::string input)
{
    servername = input;
}

std::string client::get_realname() const
{
    return (realname);
}

void client::set_realname(std::string input)
{
    realname = input;
}

time_t client::get_logtime() const
{
    return (connect_t);
}


int client::get_flag_invitation() const
{
    return (flag_invitation);
}
void client::set_flag_invitation(int flag)
{
    flag_invitation = flag;
}

int client::get_prvcount(){
    return this->prv_count;
}

void client::incr_prv(){
    this->prv_count++;
}