_This project has been created as part of the 42 curriculum by akahir, ifaleh._

---

## 📖 Description

**IRC (Internet Relay Chat)** is a real-time communication protocol created in 1988 by Jarkko Oikarinen. It is one of the first online chat systems, based on plain text sent over TCP.

- A central server manages all connections.
- Clients connect to the server via TCP.
- Communication happens through channels or private messages.

**Project Goals :**
1. Build a server capable of handling multiple simultaneous client connections following the IRC       protocol (RFC 1459)
2. Written in **C++98**.
3. Essential Features :
   - **Authentication** : Managing passwords and nicknames.
   - **Channels** : Users can JOIN, PART, and set TOPIC on channels.
   - **Privileges** : Channel Operators (ChanOps) can KICK, INVITE, or change channel modes.
   - **Communication** : Standard messaging between users (PRIVMSG) and notices.

---

## 🚀 Installation & Usage

### Compile
```bash
make
```

### Run the server
```bash
./ircserv <port> <password>
```

### Connect with netcat (for testing)
```bash
nc localhost <port>
```

### Authenticate (required before anything else)
```
PASS <password>
NICK <nickname>
USER <username> <hostname> <servername> <realname>
```
✅ Client is now connected.

---

## 💬 Commands

| Command | Usage | Description |
|---------|-------|-------------|
| `PRIVMSG` | `PRIVMSG <target> :<message>` | Send a message to a user or channel |
| `JOIN` | `JOIN <channel>[,<channel>] [<key>]` | Join a channel |
| `KICK` | `KICK <channel> <user> [<comment>]` | Remove a user from a channel |
| `INVITE` | `INVITE <nickname> <channel>` | Invite a user to a channel |
| `TOPIC` | `TOPIC <channel> [<topic>]` | View or change a channel topic |
| `MODE` | `MODE <nickname> {[+\|-]\|i\|w\|s\|o}` | Change user or channel privileges |
| `QUIT` | `QUIT [:<reason>]` | Disconnect from the server |

### MODE flags
| Flag | Description |
|------|-------------|
| `i` | Marks a user as invisible |
| `s` | User receives server notices |
| `w` | User receives wallops |
| `o` | Operator flag |


---

## 📚 Resources

- **Diagram** : [tldraw](https://www.tldraw.com/f/Aqc4EumXMXmYXmSe3xagQ?d=v-670.4002.3019.1663.page)
- **RFC 1459 Explorer** : [rfc1459-explorer](https://rfc1459-explorer.netlify.app/)
- **RFC 1459 Official** : [rfc-editor.org](https://www.rfc-editor.org/rfc/rfc1459.html)
- **ECMA-48 Colors** : [strasis.com](https://strasis.com/documentation/limelight-xe/reference/ecma-48-sgr-codes)

---

## 🤖 AI Usage

I used AI to help me understand the IRC protocol, as well as the system calls used and how they work.
