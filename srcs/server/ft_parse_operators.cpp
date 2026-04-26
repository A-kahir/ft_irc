#include "../../includes/Server.hpp"
#include "../../includes/Client.hpp"
#include "../../includes/Channels.hpp"

int ft_is_not_cmd(std::string cmd)
{
	if (cmd != "PASS" && cmd != "NICK" && cmd != "USER"
		&& cmd != "JOIN" && cmd != "KICK" && cmd != "INVITE" && cmd != "TOPIC"
		&& cmd != "MODE" && cmd != "PRIVMSG" && cmd != "KA3KOU3")
	{
		return (1);
	}
	return (0);
}

void Server::ft_operators(std::string cmd, std::string input, client &cl, int fd)
{
	if (cmd == "JOIN" && cl.Auth_done)
	{
		if (ft_parse_input_join(input, fd))
			return;
	}
	else if (cmd == "KICK" && cl.Auth_done)
	{
		if (ft_parse_input_kick(input, fd) == 1)
			return;
	}
	else if (cmd == "INVITE" && cl.Auth_done)
	{
	    if (ft_parse_input_invit(input, fd) == 1)
	        return ;
	}
	else if (cmd == "TOPIC" && cl.Auth_done)
	{
		if (ft_parse_input_topic(input, fd) == 1)
			return ;
	}
	else if (cmd == "MODE" && cl.Auth_done)
	{
		if (ft_parse_input_mode(input, fd) == 1)
			return;
		std::stringstream split(input);
		std::string chan;
		std::string mode;
		std::string arg;
		split >> chan >> mode >> arg;
		if (chan[0] == '#')
			chan.erase(0, 1);

		channels *channel = NULL;
		for (std::vector<channels>::iterator it = this->chans.begin(); it != this->chans.end(); ++it)
		{
			if (it->name == chan)
			{
				channel = &(*it);
				break;
			}
		}
		if (!channel)
			return;

		if (ft_is_operator(channel, fd) == 0)
		{
			ft_message(fd, "482", " :You're not channel operator", "#" + chan);
			return ;
		}

		if (mode == "+i")
			channel->invit_only = 1;
		else if (mode == "-i")
			channel->invit_only = 0;
		else if (mode == "+t")
			channel->topic_rest = 1;
		else if (mode == "-t")
			channel->topic_rest = 0;
		else if (mode == "+k")
			channel->pswrd = arg;
		else if (mode == "-k")
			channel->pswrd = "";
		for (size_t i = 0; i < channel->users.size(); i++)
		{
			std::string msg = ft_build_message(fd) +
							" MODE #" + chan + " " + mode;
			if (mode == "+k" || mode == "+o" || mode == "-o" || mode == "+l")
    			msg += " " + arg;
			msg += "\r\n";
			send(channel->users[i]->get_fd(), msg.c_str(), msg.size(), 0);
		}
	}
	else if (cmd == "PRIVMSG" && cl.Auth_done)
	{
		if (ft_parse_input_privmsg(input, fd) == 1)
			return ;
	}
	else if (cmd == "KA3KOU3" && cl.Auth_done)
	{
		if (!have_boot)
		{
			ft_message(fd, "", "You don't Add Ka3kou3 to this Server :(", "");
			return ;
		}
		std::stringstream split(input);
		std::string mode;
		std::string user;
		split >> mode >> user;
		printf("mode => [%s]\nuser => [%s]\n", mode.c_str(), user.c_str());
		if (mode.empty() || user.empty())
		{
			ft_message(fd, "461", " :Not enough parameters", "");
			return;
		}
		if (mode != "/logtime" && mode != "/privmsg")
		{
			ft_message(fd, "", " Ka3kou3 accept only : /logtime Or /privmsg", "");
			return ;
		}
		client *target_user = NULL;
		for (std::map<int, client>::iterator it = this->clients.begin(); it != this->clients.end(); ++it)
		{
			if (it->second.get_nick() == user)
			{
				target_user = &it->second;
				break;
			}
		}
		if (!target_user)
		{
			ft_message(fd, "401", " :No such nick", user);
			return ;
		}
		if (mode == "/logtime")
			this->ft_count_logtime_bot(fd, user, target_user->get_fd());
		if (mode == "/privmsg")
		{
			    std::stringstream otp;
					otp << "Ka3kou3 : " << user
					<< " sent " <<target_user->get_prvcount() << " PRIVMSG";
			ft_message(fd, "", otp.str() , "");
		}
	}
}
