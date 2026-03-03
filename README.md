# C++ Terminal Chat Room with File Sharing

A simple, cross-platform terminal-based chat application with file sharing capabilities. This client-server application allows multiple users to communicate in real-time and share files through a command-line interface.

## Features

- 💬 **Real-time messaging** - Broadcast messages to all connected users
- 📁 **File sharing** - Send files to all users or specific individuals
- 👤 **Private messaging** - Send direct messages to specific users
- 📋 **User list** - View currently online users
- 🔔 **Join/Leave notifications** - See when users connect or disconnect
- 🖥️ **Cross-platform** - Works on Windows, Linux, and macOS

## Requirements

- C++11 compatible compiler (g++, MinGW, etc.)
- POSIX threads library (pthread)
- Windows: MinGW with pthread support or WSL
- Linux/Mac: g++ with pthread

## Installation

### 1. Clone or download the source files
```
chat_server.cpp
chat_client.cpp
```

### 2. Compile the server
**Linux/Mac:**
```bash
g++ -std=c++11 -pthread chat_server.cpp -o chat_server
```

**Windows (MinGW):**
```bash
g++ -std=c++11 -pthread chat_server.cpp -o chat_server.exe -lws2_32
```

### 3. Compile the client
**Linux/Mac:**
```bash
g++ -std=c++11 -pthread chat_client.cpp -o chat_client
```

**Windows (MinGW):**
```bash
g++ -std=c++11 -pthread chat_client.cpp -o chat_client.exe -lws2_32
```

## Usage

### Starting the Server

Run the server on your host machine:

```bash
./chat_server
# or on Windows:
chat_server.exe
```

The server will start listening on port `8888` by default.

### Connecting Clients

In a new terminal window, connect with a username:

```bash
./chat_client <server_ip> <port> <username>
```

**Examples:**
```bash
# Connect to local server
./chat_client 127.0.0.1 8888 Alice

# Connect to remote server
./chat_client 192.168.1.100 8888 Bob
```

## Commands

Once connected, use these commands in the client:

| Command | Description | Example |
|---------|-------------|---------|
| `/help` | Show available commands | `/help` |
| `/users` | List online users | `/users` |
| `/msg <user> <message>` | Send private message | `/msg Bob Hello there!` |
| `/file <filename>` | Send file to all users | `/file document.pdf` |
| `/sendto <user> <filename>` | Send file to specific user | `/sendto Bob image.jpg` |
| `/quit` | Exit the chat | `/quit` |

## Setting Up Port Forwarding (For Public Access)

To make your chat server accessible from the internet, follow these steps:

### 1. Set a Static Local IP for Your Server Machine

**Windows:**
1. Open **Control Panel > Network and Sharing Center > Change adapter settings**
2. Right-click your active network connection > **Properties**
3. Select **Internet Protocol Version 4 (TCP/IPv4)** > **Properties**
4. Choose **"Use the following IP address"** and enter:
   - IP address: `192.168.1.50` (example - choose an unused IP)
   - Subnet mask: `255.255.255.0`
   - Default gateway: Your router's IP (usually `192.168.1.1`)

**Linux:**
Edit `/etc/netplan/00-installer-config.yaml` or use your distribution's network manager.

### 2. Configure Port Forwarding on Your Router

1. Open your router's admin page (usually `192.168.1.1` or `192.168.0.1`)
2. Log in with your admin credentials
3. Find **Port Forwarding** (may be under Advanced > NAT or Security)
4. Add a new rule:
   - **Service Name**: `Chat Server`
   - **External Port**: `8888` (or any port you prefer)
   - **Internal Port**: `8888`
   - **Internal IP**: Your server's static IP (e.g., `192.168.1.50`)
   - **Protocol**: `TCP`
5. Save and apply the settings

### 3. Configure Windows Firewall (if applicable)

1. Open **Windows Defender Firewall > Advanced Settings**
2. Click **Inbound Rules > New Rule**
3. Select **Port > Next**
4. Choose **TCP** and enter `8888` as the port
5. Select **Allow the connection**
6. Check all profiles (Domain, Private, Public)
7. Name it `Chat Server Port`

### 4. Find Your Public IP

Visit [whatismyip.com](https://whatismyip.com) or run:
```bash
curl ifconfig.me
```

### 5. Connect from the Internet

Users can now connect using your public IP:
```bash
./chat_client <your_public_ip> 8888 <username>
```

## Important Security Notes

⚠️ **WARNING**: This application has no encryption or authentication!

- **Do not use on untrusted networks** - Messages and files are sent in plain text
- **Consider adding a VPN** for secure remote access (WireGuard, OpenVPN)
- **Limit exposure** - Only keep port forwarding active when needed
- **Monitor connections** - Check logs regularly for suspicious activity

### Optional Security Improvements

For production use, consider implementing:

1. **Password authentication** - Add login system
2. **TLS/SSL encryption** - Use OpenSSL for secure connections
3. **IP whitelisting** - Restrict access to specific IPs
4. **Rate limiting** - Prevent spam and DoS attacks

## Troubleshooting

### Server won't start
```
Error: Bind failed
```
- **Solution**: Port 8888 might be in use. Change the port in the source code or close other applications.

### Clients can't connect remotely
1. Verify port forwarding is correctly configured
2. Check Windows Firewall settings
3. Confirm your public IP hasn't changed
4. Test local connection first (`127.0.0.1`)
5. Use `telnet <your_ip> 8888` to test connectivity

### File transfer fails
- Ensure you have write permissions in the client directory
- Check available disk space
- File paths with spaces? Use quotes: `/file "my document.pdf"`

### Connection drops
- Check router logs for dropped connections
- Some ISPs block incoming connections - consider using a VPN or VPS instead

## File Structure

```
chat-room/
├── chat_server.cpp      # Server source code
├── chat_client.cpp      # Client source code
├── README.md           # This file
└── received.*          # Downloaded files (created by client)
```

## Limitations

- No message history (messages aren't stored)
- No encryption (use on trusted networks only)
- Maximum simultaneous connections limited by system
- File size limited by available memory and disk space
- No resume capability for interrupted file transfers

## Future Enhancements

- [ ] Add user authentication
- [ ] Implement SSL/TLS encryption
- [ ] Add chat rooms/channels
- [ ] Message history and logging
- [ ] File transfer progress bar
- [ ] Emoji support
- [ ] Command auto-completion

## Contributing

Feel free to fork this project and submit pull requests for improvements!

## License

This project is open source and available under the MIT License.

## Support

For issues and questions:
- Check the troubleshooting section
- Verify your network configuration
- Ensure all dependencies are installed
- Test with local connections first

---
