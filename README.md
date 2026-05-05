<!-- _This project has been created as part of the 42 curriculum by akahir, ifaleh._

### Description :
the IRC (Internet Relay Chat) : is real-time communication created in 1988 by Jakko Oikarinen,
it is one of the first online chat systems. IRC is based on plain text sent over TCP.
    => A central server manages all connections.
    => Clients connect to the server via TCP.
    => communication happens throught channels or private messages.
    1 - The goal is to create a server capable of handling multiple simultaneous clients using the IRC protocol (RFC 1459).
    2 - using C++98;
    3 - Essential Features :
        --> Authentication: Managing passwords and nicknames.
        --> Channels: Users must be able to JOIN, PART, and TOPIC channels.
        --> Privileges: Implementation of "Channel Operators" (ChanOps) who can KICK, INVITE, or change channel modes.
        --> Communication: Standard messaging between users (PRIVMSG) and notices.
    
### Instructions :
    => Running server : ./ircserv <port> <password>
    => open un other terminal and run : nc localhost <port> . after runing this command 
        you can test all commands .
    => but you should first connect to the server with :
        --> pass <password>
        --> nick <nickname>
        --> user <username> <hostname> <servername> <realname>
       ==>> client connected.
    => send messages :
        --> privmsg <reciever(s)> :messages
                      /     \
        <channels members>  <user>
    => join : is used by client to start listening a specific channel
        --> join <channel>{,<channel>} [<key>{,<key>}]
    => kick : to remove a user from a channel :
        --> kick  <channel> <user> [<comment>]
    => invite : to invite users to a channel :
        --> invite <nickname> <channel>
    => topic : to change or view the topic of a channel :
        --> topic <channel> [<topic>]
    => mode : use to change chanmels or users privileges :
        --> mode <nickname> {[+|-]|i|w|s|o}
        {
           i - marks a users as invisible;
           s - marks a user for receipt of server notices;
           w - user receives wallops;
           o - operator flag.
        }
    => quit a channel
        --> quit :reason or without reason
    


### Ressources :
    tldraw :::
        https://www.tldraw.com/f/Aqc4EumXMXmYXmSe3xagQ?d=v-670.4002.3019.1663.page
    RFC(1459) :::
        https://rfc1459-explorer.netlify.app/
        https://www.rfc-editor.org/rfc/rfc1459.html
    colors :::
        https://strasis.com/documentation/limelight-xe/reference/ecma-48-sgr-codes

### AI usage : 
    
    I used AI to help me understand the IRC protocol, as well as the system calls used and how they work