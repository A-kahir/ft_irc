#include "../../includes/Client.hpp"
#include "../../includes/Server.hpp"
#include "../../includes/Channels.hpp"

bool Server::ft_is_operator(channels *chan, int fd)
{
    for (size_t i = 0; i < chan->operators.size(); i++)
    {
        if (chan->operators[i]->get_fd() == fd)
            return (1);
    }
    return (0);
}

int ft_is_valid_channel(std::string part)
{
    if (part.empty() || part[0] != '#' || part.size() == 1)
        return (0);
    for (size_t i = 0; i < part.size(); i++)
    {
        if (part[i] == ' ' || part[i] == ',' || part[i] == '\a')
            return (0);
    }
    return (1);
}

int Server::ft_parse_input_join(std::string input, int fd)
{
    if (input.empty())
        return (ft_message(fd, "461", "JOIN :Not enough parameters", "", 0), 1);
    
	client &cl = this->clients[fd];
    std::stringstream split(input);
    std::string chan_name;
    std::string key;
    split >> chan_name >> key;

	std::vector<std::string> chans = ft_split_tok(chan_name, ',');
	std::vector<std::string> keys = ft_split_tok(key, ',');

	for (size_t i = 0; i < chans.size(); i++)
	{
		if (chans[i][0] != '#')
		{
			ft_message(fd, "403", " :No such channel", cl.get_nick() + " " + chans[i], 0);
			continue;
		}
		if (ft_check_max_chans_joined(fd) > 10)
		{
			ft_message(fd, "405", " :You have joined too many channels", this->clients[fd].get_nick() + " " + chans[i], 0);
			continue;
		}
		if (!ft_is_valid_channel(chans[i]))
		{
			ft_message(fd, "403", " :No such channel", cl.get_nick() + " " + chans[i], 0);
			continue;
		}
		if (chans[i].size() > 200)
		{
			ft_message(fd, "403", " :No such channel", cl.get_nick() + " " + chans[i], 0);
			continue;
		}
		if (chans[i][0] == '#')
       		chans[i].erase(0, 1);
		
		channels *chan = ft_get_channel(chans[i]);
		if (chan)
		{
			if (!ft_is_user_in_channel(chan, fd))
			{
				if (chan->invit_only && cl.get_flag_invitation() == 0)
				{
					ft_message(fd, "473", " :Cannot join channel (+i)", "#" + chan->name, 0);
					continue;
				}
				std::string curr_key;
				if (i < keys.size())
					curr_key = keys[i];
				else
					curr_key = "";
				if (!chan->pswrd.empty())
				{
					if (curr_key.empty() || curr_key != chan->pswrd)
					{
						ft_message(fd, "475", " :Cannot join channel (+k)", "#" + chan->name, 0);
						continue;
					}
				}
				if (chan->user_limit != -1 && chan->users.size() >= (size_t)chan->user_limit)
				{
					ft_message(fd, "471", " :Cannot join channel (+l)", "#" + chan->name, 0);
					continue;
				}
			}
		}

		if (!chan)
		{
			channels new_chan;
			new_chan.name = chans[i];
			new_chan.users.push_back(&cl);
			new_chan.operators.push_back(&cl);
			new_chan.pswrd = "";
			this->chans.push_back(new_chan);
			chan = &this->chans.back();
		}
		else
		{
			bool exist = 0;
			for (size_t i = 0; i < chan->users.size(); i++)
			{
				if (chan->users[i] == &cl)
				{
					exist = 1;
					break;
				}
			}
			if (exist)
            	continue;
			chan->users.push_back(&cl);
		}
		std::string join_broadcast =  ft_build_message(fd) + " JOIN :#" + chan->name + "\r\n";
		for (size_t i = 0; i < chan->users.size(); i++)
		{
			send(chan->users[i]->get_fd(), join_broadcast.c_str(), join_broadcast.size(), 0);
		}

		std::string message = ":" SERVER_NAME " 332 " + cl.get_nick() + " #" + chan->name + " :Welcome to your Server\r\n";
		send(fd, message.c_str(), message.size(), 0);
		std::string join_msg = cl.get_nick() + " = #" + chan->name + " :";
		for (size_t i = 0; i < chan->users.size(); i++)
		{
			if (ft_is_operator(chan, chan->users[i]->get_fd()) == 1)
            	join_msg += "@" + chan->users[i]->get_nick();
			else
				join_msg += chan->users[i]->get_nick();
			join_msg += " ";
		}
    	ft_message(fd, "353 ", join_msg, "", 1);

    	std::string end_msg = ":" SERVER_NAME " 366 " + cl.get_nick() + " #" + chan->name + " :End of /NAMES list\r\n";
    	send(fd, end_msg.c_str(), end_msg.size(), 0);
	}
    return (0);
}

int Server::ft_parse_input_kick(std::string input, int fd) 
{
    if (input.empty())
        return (ft_message(fd, "461", " KICK :Not enough parameters", "", 0), 1);

    std::stringstream ss(input);
    std::string chan_name;
    std::string users;
    std::string reason;
    ss >> chan_name >> users;
    std::getline(ss, reason);
    if (chan_name.empty() || users.empty())
        return (ft_message(fd, "461", " KICK :Not enough parameters", "", 0), 1);
    int found = 0;
	if (chan_name[0] == '#')
    	chan_name.erase(0, 1);
    for (std::vector<channels>::iterator it = this->chans.begin(); it != this->chans.end(); ++it)
    {
        if (it->name == chan_name)
        {
            found = 1;
            break;
        }
    }
    if (!found)
    {
        ft_message(fd, "403", " :No such channel", chan_name, 0);
        return (1);
    }
    else
    {
        int flag = 0;
        
        for (std::map<int, client>::iterator iter = this->clients.begin(); iter != this->clients.end(); iter++)
        {
            if (iter->second.get_nick() == users)
            {
                flag = 1;
                break;
            }
        }
        if (!flag)
            return (ft_message(fd, "442", ":You're not on that channel", input, 0), 1);
    }
    size_t start = 0;
    while (start < users.size())
    {
        size_t comma = users.find(',', start);
        std::string nick;
        if (comma == std::string::npos)
            nick = users.substr(start);
        else
            nick = users.substr(start, comma - start);

        std::map<int, client>::iterator it_user = this->clients.find(fd);
        if (it_user == this->clients.end())
            ft_message(fd, "401", " :No such nick", nick, 0);
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
	channels *chans = ft_get_channel(chan_name);
	if (ft_is_operator(chans, fd) == 0)
	{
		return (ft_message(fd, "482", " :You're not channel operator", chan_name, 0), 1);
	}
	ft_get_user_by_nick(users)->set_flag_invitation(0);
	for (size_t i = 0; i < chans->users.size(); i++)
	{
		std::string msg = ft_build_message(fd) + " KICK #" + chan_name + " " + users;
		if (!reason.empty())
			msg += reason;
		msg += "\r\n";
		send(chans->users[i]->get_fd(), msg.c_str(), msg.size(), 0);
	}
	for (std::vector<channels>::iterator iter = this->chans.begin(); iter != this->chans.end(); iter++)
	{
		if (iter->name == chan_name)
		{
			for (std::vector<client*>::iterator it = iter->users.begin();
				it != iter->users.end(); it++)
			{
				if ((*it)->get_nick() == users)
				{
					if (ft_is_operator(&(*iter), (*it)->get_fd()) == 1)
					{
						for (std::vector<client*>::iterator op_it = iter->operators.begin();
							op_it != iter->operators.end(); op_it++)
						{
							if (*op_it == *it)
							{
								iter->operators.erase(op_it);
								break;
							}
						}
					}
					iter->users.erase(it);
					if (iter->operators.empty() && !iter->users.empty())
						iter->operators.push_back(iter->users.front());
					if (iter->users.empty())
						this->chans.erase(iter);
					return (0);
				}
			}
		}
	}
    return (0);
}


int Server::ft_parse_input_invit(std::string input, int fd)
{
    if (input.empty())
        return (ft_message(fd, "461", " INVITE :Not enough parameters", "", 0), 1);

    std::stringstream split(input);
    std::string user;
    std::string chan;
    split >> user >> chan;
    if (user.empty() || chan.empty())
        return (ft_message(fd, "461", " INVITE :Not enough parameters", "", 0), 1);
    if (!chan.empty() && chan[0] == '#')
        chan.erase(0, 1);
    client *target_user = NULL;

    for (std::map<int, client>::iterator it = this->clients.begin();
         it != this->clients.end(); ++it)
    {
        if (it->second.get_nick() == user)
        {
            target_user = &it->second;
            break;
        }
    }
    if (!target_user)
        return (ft_message(fd, "401", " :No such nick", user, 0), 1);

    channels *target_chan = ft_get_channel(chan);
    if (!target_chan)
	{
		return (ft_message(fd, "403", " :No such channel", "#" + chan, 0), 1);
	}
	if (ft_is_operator(target_chan, fd) == 0)
		return (ft_message(fd, "482", " :You're not channel operator", "#" + chan, 0), 1);

    int is_in_channel = 0;
    for (size_t i = 0; i < target_chan->users.size(); i++)
    {
        if (target_chan->users[i] == &this->clients[fd])
        {
            is_in_channel = 1;
            break;
        }
    }
    if (!is_in_channel)
        return (ft_message(fd ,"442", " :You're not on that channel", "#" + chan, 0), 1);
    for (size_t i = 0; i < target_chan->users.size(); i++)
    {
        if (target_chan->users[i] == target_user)
        {
            return (ft_message(fd, "443", " :is already on channel", user + " #" + chan, 0), 1);
        }
    }
    if (is_in_channel)
    {
		this->clients[target_user->get_fd()].set_flag_invitation(1);

        std::string message_sender = ":" SERVER_NAME " 341 " + this->clients[fd].get_nick() + " #"
                + target_chan->name + " " + target_user->get_nick() + "\r\n";
                send(fd, message_sender.c_str(), message_sender.size(), 0);

        std::string message_getter = ft_build_message(fd) + " INVITE "
                + target_user->get_nick() + " :#" + target_chan->name + "\r\n";
                send(target_user->get_fd(), message_getter.c_str(), message_getter.size(), 0);
    }
    return (0);
}

int Server::ft_parse_input_topic(std::string input, int fd)
{
	if (input.empty())
        return (ft_message(fd, "461", " TOPIC :Not enough parameters", "", 0), 1);

	int flag_set = 0;
	std::vector<std::string> result;
	std::istringstream tmp(input);
	std::string word;
	while (tmp >> word)
	{
		if (word[0] == ':')
		{
			flag_set = 1;
			word.erase(0, 1);
			std::string rest;
			std::getline(tmp, rest);
			word += rest;
			result.push_back(word);
			break;
		}
		result.push_back(word);
	}
	if (result.size() < 1)
		return (ft_message(fd, "461", " TOPIC :Not enough parameters", "", 0), 1);

	std::string chan = result[0];
	std::vector<std::string> all_chans = ft_split_tok(chan, ',');
	std::string topic_msg;
	if (result.size() > 1)
		topic_msg = result[1];
	channels *channel = NULL;
	for (size_t i = 0; i < all_chans.size(); i++)
	{
		channel = NULL;
		if (all_chans[i][0] == '#')
			all_chans[i].erase(0, 1);
		else if (all_chans[i][0] != '#')
			ft_message(fd, "403", " :No such channel", all_chans[i], 0);

		for (std::vector<channels>::iterator it = this->chans.begin(); it != this->chans.end(); it++)
		{
			if (it->name == all_chans[i])
			{
				channel = &(*it);
				break;
			}
		}
		if (!channel)
		{
			ft_message(fd, "403", " :No such channel", "#" + all_chans[i], 0);
			continue;
		}
		
		if (!ft_is_user_in_channel(channel, fd))
		{
			ft_message(fd, "442", " :You're not on that channel", "#" + all_chans[i], 0);
			continue;
		}

		if (!flag_set)
		{
			for (std::vector<channels>::iterator it = this->chans.begin(); it != this->chans.end(); it++)
			{
				if (it->name == all_chans[i])
				{
					if (it->topic == "No topic is set")
						ft_message(fd, "331", " :No topic is set", "#" + all_chans[i], 0);
					else
						ft_message(fd, "332", " :" + it->topic, "#" + all_chans[i], 0);
					break;
				}
			}
		}
	}
	if (!flag_set)
		return (0);
	if (channel && channel->topic_rest && ft_is_operator(channel, fd) == 0)
		return (ft_message(fd, "482", " :You're not channel operator", " #" + chan, 0), 1);
	else if (flag_set && channel)
	{
		channel->topic = topic_msg;
		
		for (size_t i = 0; i < channel->users.size(); i++)
		{
			client *user = channel->users[i];
			std::string msg = ft_build_message(fd) +" TOPIC " + chan + " :" + topic_msg + "\r\n";
			send(user->get_fd(), msg.c_str(), msg.size(), 0);
		}
	}
	return (0);
}

int Server::ft_parse_input_mode(std::string input, int fd)
{
	if (input.empty())
		return (ft_message(fd, "461", " MODE :Not enough parameters", "", 0), 1);

	std::stringstream split(input);
	std::string chan;
	std::string mode;
	std::string arg;
	split >> chan >> mode >> arg;
	if (chan.empty() || mode.empty())
		return (ft_message(fd, "461", " MODE :Not enough parameters", "", 0), 1);
	if (chan[0] == '#')
		chan.erase(0, 1);
	else if (chan[0] != '#')
		return (ft_message(fd, "403", " :No such channel", "#" + chan, 0), 1);

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
		return (ft_message(fd, "403", " :No such channel", "#" + chan, 0), 1);

	if (!ft_is_user_in_channel(channel, fd))
		return (ft_message(fd, "442", " :You're not on that channel", "#" + chan, 0), 1);

	if (mode != "+i" && mode != "-i" &&
		mode != "+t" && mode != "-t" &&
		mode != "+k" && mode != "-k" &&
		mode != "+o" && mode != "-o" &&
		mode != "+l" && mode != "-l")
		return (ft_message(fd, "472", " :is unknown mode char to me", mode, 0), 1);

	if (ft_is_operator(channel, fd) == 0)
	{
		ft_message(fd, "482", " :You're not channel operator", "#" + chan, 0);
		return (1);
	}
	if ((mode == "+k" || mode == "+o" || mode == "+l") && arg.empty())
    	return (ft_message(fd, "461", " MODE :Not enough parameters", "", 0), 1);
	else if (mode == "+o" || mode == "-o")
	{
		client *target = NULL;
		for (size_t i = 0; i < this->clients.size(); i++)
		{
			if (this->clients[i].get_nick() == arg)
			{
				target = &this->clients[i];
				break;
			}
		}
		if (!target)
			return (ft_message(fd, "401", " :No such nick", arg, 0), 1);

		if (!ft_is_user_in_channel(channel, target->get_fd()))
			return (ft_message(fd, "441", " :They aren't on that channel", target->get_nick() + " #" + chan, 0), 1);

		if (mode == "+o")
		{
			if (!ft_is_operator(channel, target->get_fd()))
				channel->operators.push_back(target);
		}
		else
		{
			for (size_t i = 0; i < channel->operators.size(); i++)
			{
				if (channel->operators[i] == target)
				{
					channel->operators.erase(channel->operators.begin() + i);
					break;
				}
			}
			if (channel->operators.empty() && channel->users.size() > 0)
			{
				for (size_t j = 0; j < channel->users.size(); j++)
				{
					if (channel->users[j] != target)
					{
						channel->operators.push_back(channel->users[j]);
						break;
					}
				}
			}
		}
	}
	else if (mode == "+l")
		channel->user_limit = atoi(arg.c_str());
	else if (mode == "-l")
		channel->user_limit = -1;

	return (0);
}

int Server::ft_parse_input_privmsg(std::string input, int fd)
{
    if (input.empty())
        return (ft_message(fd, "461", " PRIVMSG :Not enough parameters", "", 0), 1);
    
    std::stringstream split(input);
    std::string all_targets;
	split >> all_targets;
	std::string message;
	std::getline(split, message);
	while (!message.empty() && message[0] == ' ')
		message.erase(0, 1);
	if (!message.empty() && message[0] == ':')
		message.erase(0, 1);
	if (message.empty())
		return (ft_message(fd, "412", " PRIVMSG :No text to send", "", 0), 1);
    std::vector<std::string> targets = ft_split_tok(all_targets, ',');
    for (size_t i = 0; i < targets.size(); i++)
    {
        std::string current_target = targets[i];
        if (current_target.empty())
            continue;
        if (current_target[0] == '#' || current_target[0] == '&')
        {
            std::string chan_name = current_target;
            if (chan_name[0] == '#') chan_name.erase(0, 1);

            channels *target_chan = ft_get_channel(chan_name);
            if (!target_chan)
            {
                ft_message(fd, "403", " :No such channel", current_target, 0);
                continue;
            }
            int is_in_channel = 0;
            for (size_t j = 0; j < target_chan->users.size(); j++)
            {
                if (target_chan->users[j]->get_fd() == fd)
                {
                    is_in_channel = 1;
                    break;
                }
            }
            if (!is_in_channel)
            {
                ft_message(fd, "442", " :You're not on that channel", current_target, 0);
                	continue;
            }

            for (size_t j = 0; j < target_chan->users.size(); j++)
            {
                if (target_chan->users[j]->get_fd() != fd)
                {
                    std::string msg = ft_build_message(fd) + " PRIVMSG " + current_target + " :" + message + "\r\n";
					send(target_chan->users[j]->get_fd(), msg.c_str(), msg.size(), 0);
                }
            }
        }
        else
        {
            client *target_user = NULL;
            for (size_t j = 0; j < this->clients.size(); j++)
            {
                if (this->clients[j].get_nick() == current_target)
                {
                    target_user = &this->clients[j];
                    break;
                }
            }
            if (!target_user)
            {
                ft_message(fd, "401", " :No such nick", current_target, 0);
                continue;
            }
			this->clients[fd].incr_prv();
            std::string msg = ft_build_message(fd) + " PRIVMSG " + target_user->get_nick() + " :" + message + "\r\n";
            send(target_user->get_fd(), msg.c_str(), msg.size(), 0);
        }
    }
    return (0);
}
