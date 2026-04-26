#include "../../includes/Server.hpp"
#include "../../includes/Client.hpp"
#include "../../includes/Channels.hpp"

void ft_message(int fd, std::string code_rfc, std::string message, std::string data)
{
    std::string print = code_rfc + message;
    if (!data.empty())
        print = code_rfc + " :" + data + message;

    send(fd, print.c_str(), print.size(), 0);
}

void Server::ft_count_logtime_bot(int fd, std::string user, int target_fd)
{
    time_t now = std::time(NULL);


    int result = now - this->clients[target_fd].get_logtime();

    int min = result / 60;
    int sec = result % 60;

    std::stringstream otp;
    otp << "Ka3kou3 : Logtime of " << user
       << " is : " << min << "min " << sec << "sec\n";

    ft_message(fd, "", otp.str(), "");
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
