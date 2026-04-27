#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <iostream>
#include <poll.h>
#include <map>
#include <vector>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include "Client.hpp"

#define SERVER_NAME "hassan&houssien"

class client;
class channels;

class Server
{
    private :
        int port;
        int socket_fd;
        std::string password;
        bool isStopped;
        
        std::vector<struct pollfd> fds;
        std::map<int , client> clients;
        std::vector<channels> chans;
    public :
        Server(int port, std::string password);
        void Start();
        void NewClient();
        void handle_message(int fd);
        void RemoveClient(int fd);
        void process_commands(int fd);

        std::string get_password() const;
        int get_fd();
        ~Server();

        // main func of commands:
        void ft_first_cmds(std::string cmd, std::string input, client &cl, int fd);
        
        // parsign Auth commands :
        int ft_parse_input_pass(std::string input, int fd);
        int ft_parse_input_nick(std::string input, client *cl, int fd);
        int ft_parse_input_user(std::string input, client *cl, int fd);
        
        // main func of operators:
        void ft_operators(std::string cmd, std::string input, client &cl, int fd);
        bool ft_is_operator(channels *chan, int fd);

        // parsing operators :
        int ft_parse_input_join(std::string input, int fd);
        int ft_parse_input_kick(std::string input, int fd);
        int ft_parse_input_invit(std::string input, int fd);
        int ft_parse_input_topic(std::string input, int fd);
        int ft_parse_input_mode(std::string input, int fd);
        int ft_parse_input_privmsg(std::string input, int fd);

        // helper functions :
        channels* ft_get_channel(std::string name);
        client* ft_get_user_by_nick(std::string nick);
        bool ft_is_user_in_channel(channels *chan, int fd);
        std::string ft_build_message(int fd);
        int ft_check_max_chans_joined(int fd);

        //Bot
        bool have_boot;
        void ft_count_logtime_bot(int fd, std::string user, int target_fd);
};

#endif