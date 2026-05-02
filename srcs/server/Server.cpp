#include "../../includes/Server.hpp"
#include "../../includes/Channels.hpp"
 
Server::Server(int port, std::string password) : port(port), password(password)
{
	socket_fd = -1;
	have_boot = 0;
}

int Server::get_fd(){
	return this->socket_fd;
}

void Server::NewClient(){
	struct sockaddr_in client_addr;
	std::memset(&client_addr, 0, sizeof(client_addr));
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(this->socket_fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_fd == -1)
	{
		std::cerr << "Error accept failed :(\n";
		return ;
	}
	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1)
		std::cerr << "Error fcntl failed :(" << std::endl;
	struct pollfd cl;
	cl.fd = client_fd;
	cl.events = POLLIN;
	cl.revents = 0;
	this->fds.push_back(cl);
	client newClient;
	this->clients[client_fd] = newClient;
}

void Server::RemoveClient(int fd)
{
	std::string nickname = this->clients[fd].get_nick();
    client *cl = &this->clients[fd];
	if (have_boot)
    	this->have_boot = 0;
    for (std::vector<channels>::iterator it = this->chans.begin(); it != this->chans.end();)
    {
        channels &chan = *it;
        for (std::vector<client*>::iterator usr = chan.users.begin(); usr != chan.users.end();)
        {
            if (*usr == cl)
                usr = chan.users.erase(usr);
            else
                ++usr;
        }
        for (std::vector<client*>::iterator opr = chan.operators.begin(); opr != chan.operators.end();)
        {
            if (*opr == cl)
                opr = chan.operators.erase(opr);
            else
                ++opr;
        }
        if (chan.users.empty())
        {
            it = this->chans.erase(it);
            continue;
        }
        if (chan.operators.empty() && !chan.users.empty())
        {
            chan.operators.push_back(chan.users.front());
        }
        ++it;
    }
    close(fd);
    this->clients.erase(fd);
    for (std::vector<pollfd>::iterator it = this->fds.begin();
         it != this->fds.end(); ++it)
    {
        if (it->fd == fd)
        {
            this->fds.erase(it);
            break;
        }
    }
    std::cout << RED << "==>  Client " << fd << " " << UNDERLINE << nickname << RESET_UNDERLINE << " Disconnected" << RESET_COLOR << std::endl;
}

std::string Server::get_password() const
{
	return (password);
}

void Server::process_commands(int fd)
{
	std::string& buffer = this->clients[fd].get_buffer();
	size_t pos;
	while((pos = buffer.find('\n')) != std::string::npos)
	{
		std::string command = buffer.substr(0, pos);
		if (!command.empty() && command[command.size() - 1] == '\r')
			command.erase(command.size() - 1);
		buffer.erase(0,  pos + 1);
		if (command.length() + 2 > 512)
			return;
		std::istringstream save(command);
		std::string cmd;
		if (!(save >> cmd))
			continue;
		std::string input;
		std::getline(save, input);
		if (!input.empty() && input[0] == ' ')
			input.erase(0, 1);
		for (size_t i = 0; i < cmd.size(); i++)
			cmd[i] = toupper(cmd[i]);
		this->clients[fd].set_fd(fd);
		if (ft_is_not_cmd(cmd) == 1)
		{
			ft_message(fd, "421", " :Unknown command", cmd);
			continue;
		}
		else
		{
			ft_first_cmds(cmd, input, this->clients[fd], fd);
			if (this->clients.find(fd) == this->clients.end())
                return;
			if (this->clients[fd].Auth_is_registered() == 1 && this->clients[fd].Auth_done == 0)
			{
				std::cout << GREEN << "==>  Client " << fd << " " << UNDERLINE << this->clients[fd].get_nick() << RESET_UNDERLINE << " Connected" << RESET_COLOR << std::endl;
			}
			if (this->clients[fd].Auth_is_registered() == 1)
			{
				this->clients[fd].Auth_done = 1;
				ft_operators(cmd, input, this->clients[fd], fd);
				if (this->clients.find(fd) == this->clients.end())
                	return;
			}
		}
	}
}

void Server::handle_message(int fd)
{
	char buffer[1024];
	std::memset(buffer, 0, sizeof(buffer));
	ssize_t bytesRead = recv(fd, buffer, sizeof(buffer) - 1, 0);
	if (bytesRead <= 0)
	   this->RemoveClient(fd);
	else
	{
		this->clients[fd].appendToBuffer(std::string(buffer));
		this->process_commands(fd);
	}
}

void Server::Start(){
	this->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (this->socket_fd == -1)
		std::cerr << "Error : socket failed :(" << std::endl;
	int opt = 1;
	if (setsockopt(this->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
		std::cerr << "Error : setsockopt failed :(" << std::endl;
	if (fcntl(this->socket_fd, F_SETFL, O_NONBLOCK) == -1)
		std::cerr << "Error : fcntl failed :(" << std::endl;
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(this->port);
	if (bind(this->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		std::cerr << "ERROR: can't bind :(" << std::endl;
		return ;
	}
	if (listen(this->socket_fd, SOMAXCONN) == -1)
		std::cerr << "ERROR: listen failed :(" << std::endl;
	struct pollfd server;
	server.fd = this->socket_fd;
	server.events = POLLIN;
	server.revents = 0;
	this->fds.push_back(server);
	std::string text = "IRC server started. Waiting for connection...";
	std::cout << GREEN;
	for (size_t i = 0; i < text.size(); i++)
	{
		std::cout << text[i] << std::flush;
		usleep(40000);
	}
	std::cout << RESET_COLOR << std::endl;
	while (1)
	{
		if (poll(&this->fds[0], this->fds.size(), -1) == -1)
		{
			std::cerr << "Error : Poll failed:(\n";
			break;
		}
		for (size_t i = 0; i < fds.size(); i++)
		{
			if (fds[i].revents & POLLIN){
				if (fds[i].fd == this->socket_fd)
					this->NewClient();
				else
					this->handle_message(fds[i].fd);
			}
			else if (fds[i].revents & (POLLHUP | POLLERR))
				this->RemoveClient(fds[i].fd);
		}
	}
}

Server::~Server()
{}
