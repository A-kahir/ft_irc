#include "../../includes/Server.hpp"
#include "../../includes/Client.hpp"
#include "../../includes/Channels.hpp"

void ft_message(int fd, std::string code_rfc, std::string message, std::string data, int flag)
{
    std::string print;
    if (flag == 1)
    {
       std::string server_name =  SERVER_NAME;
       print = ":" +  server_name + " ";
    }
    print += code_rfc;
    if (!data.empty())
        print += " " + data;
    if (!message.empty())
        print += message;
    print += "\r\n";
    send(fd, print.c_str(), print.size(),0);
}

void Server::ft_count_logtime_bot(int fd, std::string user, int target_fd)
{
    time_t now = std::time(NULL);


    int result = now - this->clients[target_fd].get_logtime();

    int min = result / 60;
    int sec = result % 60;

    std::stringstream otp;
    otp << "Ka3kou3 : Logtime of " << user
       << " is : " << min << "min " << sec << "sec";

    ft_message(fd, "", otp.str(), "", 0);
}

std::vector<std::string> ft_split_tok(const std::string chanls, char delimiter)
{
	std::vector<std::string> full_tokens;
	std::string token;
	std::stringstream split(chanls);
	while (std::getline(split, token, delimiter))
	{
		if (!token.empty())
			full_tokens.push_back(token);
	}
	return (full_tokens);
}
