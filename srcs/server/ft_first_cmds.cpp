#include "../../includes/Client.hpp"
#include "../../includes/Server.hpp"

void Server::ft_first_cmds(std::string cmd, std::string input, client &cl, int fd)
{
    if ((cmd == "JOIN" || cmd == "INVITE" || cmd == "KICK" || cmd == "MODE" || cmd == "TOPIC" || cmd == "PRIVMSG")
        && this->clients[fd].Auth_is_registered() == 0)
    {
        ft_message(fd, "451", " :You have not registered", "");
        return;
    }
    if (cmd == "PASS")
    {
        if (cl.Auth_done)
        {
            ft_message(fd, "462", " :You may not reregister", "");
            return;
        }
        else
        {
            if (ft_parse_input_pass(input, fd) == 1)
                return;
            else
                cl.Auth_pass = 1;
        }
    }
    else if (cmd == "NICK")
    {
        if (cl.Auth_done)
        {
            ft_message(fd, "462", " :You may not reregister", "");
                return;
        }
        if (input.empty() || input[0] == ' ')
        {
            ft_message(fd, "461", " NICK :Not enough parameters", "");
                return;
        }
        if (!cl.Auth_pass)
        {
            ft_message(fd, "451", " :You have not registered", "");
                return;
        }
        else
        {
            for (std::map<int, client>::iterator it = this->clients.begin(); it != this->clients.end(); ++it)
            {
                if (it->second.get_nick() == input)
                {
                    ft_message(fd, "433", " :Nickname is already in use", "");
                    return;
                }
            }
            if (ft_parse_input_nick(input, &cl, fd) == 1)
                return;
            else
            {
                std::string checker = input;
                for (size_t i = 0; i < checker.size(); i++)
                    checker[i] = std::toupper(checker[i]);
                if (checker == "BOT")
                {
                    if (this->have_boot == 1)
                        ft_message(fd, "", "You Can't Add More than one Ka3kou3 ;)", "");
                    this->have_boot = 1;
                    cl.set_nick("ka3kou3");
                    cl.Auth_nick = 1;
                    cl.Auth_user = 1;
                    cl.set_user("Bot");
                    cl.set_hostname("0");
                    cl.set_servername("0");
                    cl.set_realname("ka3kou3_Bot");
                    std::string msg = "You Add Ka3kou3 to your Server\r\nYou can use :\r\n   -> Ka3kou3 /logtime <username> : to check logtime\r\n   -> Ka3kou3 /privmsg <username> : to check number of message you send";
                    ft_message(fd, "", msg, "");
                    return;
                }
                cl.set_nick(input);
                cl.Auth_nick = 1;
            }
        }
    }
    else if (cmd == "USER")
    {
        if (!cl.Auth_pass || !cl.Auth_nick)
        {
            ft_message(fd, "451", " :You have not registered", "");
            return;
        }
        if (cl.Auth_done)
        {
            ft_message(fd, "462", " :You may not reregister", "");
            return;
        }
        else
        {
            if (ft_parse_input_user(input, &cl, fd) == 1)
                return;
            else
            {
                cl.Auth_user = 1;
                std::string msg1 = "001 " + this->clients[fd].get_nick() + " :Welcome to the IRC Network " + this->clients[fd].get_nick() + "!" + this->clients[fd].get_user() + "@localhost\n"
                                ":" SERVER_NAME " 002 " + this->clients[fd].get_nick() + " :Your host is server, running version 1.0\r\n"
                                + ":" SERVER_NAME " 003 " + this->clients[fd].get_nick() + " :This server was created Today\r\n"
                                + ":" SERVER_NAME" 004 " + this->clients[fd].get_nick() + " server 1.0";
            
            	ft_message(fd, "", msg1, "");
            }
        }

    }
    
}
