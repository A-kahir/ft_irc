#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>
#include <ctime>

#include <stdio.h>

class channels;

class client
{
    private:
        int fd;
        int prv_count;
        std::string buffer;
        std::string nick;
        std::string user;
        std::string hostname;
        std::string servername;
        std::string realname;
        time_t connect_t;

        int flag_invitation;

    public:
        client();
        ~client();
        bool Auth_pass;
        bool Auth_nick;
        bool Auth_user;
        bool Auth_done;
        bool Auth_is_registered();

        void appendToBuffer(const std::string data);
        std::string& get_buffer();

        int get_fd() const;
        void set_fd(int new_fd);

        int get_prvcount();
        void incr_prv();

        std::string get_nick() const;
        void set_nick(std::string input);

        std::string get_user() const;
        void set_user(std::string input);

        std::string get_hostname() const;
        void set_hostname(std::string input);

        std::string get_servername() const;
        void set_servername(std::string input);

        std::string get_realname() const;
        void set_realname(std::string input);

        int get_flag_invitation() const;
        void set_flag_invitation(int flag);

        //bot
        time_t get_logtime() const;
};

//global functions :
int ft_parse_args(int argc, char **argv);
void ft_message(int fd, std::string code_rfc, std::string message, std::string data, int flag);
int ft_is_not_cmd(std::string cmd);
std::vector<std::string> ft_split_tok(const std::string chanls, char delimiter);

#endif