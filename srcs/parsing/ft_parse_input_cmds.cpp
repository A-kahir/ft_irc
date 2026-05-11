#include "../../includes/Server.hpp"
#include "../../includes/Client.hpp"

int Server::ft_parse_input_pass(std::string input, int fd)
{
    if (input.empty() || input[0] == ' ')
    {
        ft_message(fd, "461", " PASS :Not enough parameters", "", 0);
        return (1);
    }
    else if (input != this->get_password())
    {
        ft_message(fd, "464", " :Password incorrect", "", 0);
        return (1);
    }
    return (0);
}

int Server::ft_parse_input_nick(std::string input, client *cl, int fd)
{
    if (input.empty())
    {
        ft_message(fd, "431", " :No nickname given", "", 0);
        return (1);
    }
    else if (input[0] == ' ')
    {
        ft_message(fd, "461", " :Not enough parameters", "", 0);
        return (1);
    }
    for (size_t i = 0; i < input.length(); i++)
    {
        if ((!isalnum(input[i]) && input[i] != '-' &&
            input[i] != '[' && input[i] != ']' &&
            input[i] != '\\' && input[i] != '`' &&
            input[i] != '^' && input[i] != '{' &&
            input[i] != '}') || input[i] == ' ')
        {
            ft_message(fd, "432", " :Erroneus nickname", input, 0);
            return (1);
        }
    }
    if (input.length() > 9)
    {
        ft_message(fd, "432", " :Erroneus nickname", input, 0);;
        return (1);
    }
    cl->set_nick(input);
    return (0);
}

int Server::ft_parse_input_user(std::string input, client *cl, int fd)
{
    if (input.empty())
    {
        ft_message(fd, "461", " USER :Not enough parameters", "", 0);
        return (1);
    }

    while (!input.empty() && input[0] == ' ')
        input.erase(0, 1);
    while (!input.empty() && input[input.length() - 1] == ' ')
        input.erase(input.length() - 1, 1);

    std::vector<std::string> result;
    std::stringstream tmp(input);
    std::string word;
    while (tmp >> word)
    {
        if (word[0] == ':')
        {
            word.erase(0, 1);
            std::string rest;
            std::getline(tmp, rest);
            word += rest;
            result.push_back(word);
            break;
        }
        result.push_back(word);
    }
    if (result.size() < 4)
    {
        ft_message(fd, "461", " USER :Not enough parameters", "", 0);
        return (1);
    }
    cl->set_user(result[0]);
    cl->set_hostname("localhost");
    cl->set_servername(SERVER_NAME);
    cl->set_realname(result[3]);
    return (0);
}
