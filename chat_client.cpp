#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <map>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <conio.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
    typedef int socklen_t;
    
    // For Windows, use the built-in functions
    #define KEY_ENTER 13
    #define KEY_BACKSPACE 8
    #define GET_CHAR() _getch()
    #define KEYBOARD_HIT() _kbhit()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/select.h>
    #include <termios.h>
    #include <fcntl.h>
    #define CLOSE_SOCKET close
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define KEY_ENTER '\n'
    #define KEY_BACKSPACE 127
    
    // For Linux/Mac, we need to implement these
    int KEYBOARD_HIT() {
        struct termios oldt, newt;
        int ch;
        int oldf;

        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

        ch = getchar();

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);

        return ch != EOF;
    }

    char GET_CHAR() {
        struct termios oldt, newt;
        char ch;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ch;
    }
#endif

const int BUFFER_SIZE = 4096;
const int FILE_BUFFER_SIZE = 8192;

enum MessageType {
    MSG_CHAT = 1,
    MSG_FILE_META = 2,
    MSG_FILE_DATA = 3,
    MSG_FILE_ACK = 4,
    MSG_JOIN = 5,
    MSG_LEAVE = 6,
    MSG_PRIVATE = 7
};

struct FileMetadata {
    char filename[256];
    char sender[50];
    char target[50];
    unsigned long long filesize;
    int total_chunks;
    int current_chunk;
};

SOCKET client_socket;
std::string username;
std::atomic<bool> running{true};

bool init_sockets() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void receive_messages() {
    char buffer[FILE_BUFFER_SIZE];
    
    while (running) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            if (running) {
                std::cout << "\nDisconnected from server." << std::endl;
                running = false;
            }
            break;
        }
        
        buffer[bytes_received] = '\0';
        std::cout << "\r" << buffer << std::endl;
        std::cout << "> " << std::flush;
    }
}

void send_file(const std::string& filename, const std::string& target = "all") {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cout << "Cannot open file: " << filename << std::endl;
        return;
    }
    
    unsigned long long filesize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Send file metadata
    FileMetadata meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.filename, filename.c_str(), sizeof(meta.filename) - 1);
    strncpy(meta.sender, username.c_str(), sizeof(meta.sender) - 1);
    strncpy(meta.target, target.c_str(), sizeof(meta.target) - 1);
    meta.filesize = filesize;
    meta.total_chunks = static_cast<int>((filesize + FILE_BUFFER_SIZE - 1) / FILE_BUFFER_SIZE);
    meta.current_chunk = 0;
    
    int msg_type = MSG_FILE_META;
    send(client_socket, reinterpret_cast<char*>(&msg_type), sizeof(int), 0);
    send(client_socket, reinterpret_cast<char*>(&meta), sizeof(FileMetadata), 0);
    
    // Send file data in chunks
    char file_buffer[FILE_BUFFER_SIZE];
    int chunk_count = 0;
    
    std::cout << "Sending file: " << filename << " (" << filesize << " bytes)" << std::endl;
    
    while (!file.eof()) {
        file.read(file_buffer, FILE_BUFFER_SIZE);
        int bytes_read = static_cast<int>(file.gcount());
        
        if (bytes_read > 0) {
            send(client_socket, file_buffer, bytes_read, 0);
            chunk_count++;
            
            // Update progress
            float progress = (static_cast<float>(chunk_count) * FILE_BUFFER_SIZE / filesize) * 100;
            if (progress > 100) progress = 100;
            
            std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                      << progress << "%" << std::flush;
        }
    }
    
    std::cout << "\nFile sent successfully: " << filename << std::endl;
    file.close();
}

// Availble options output
void print_help() {
    std::cout << "\n=== Commands ===" << std::endl;
    std::cout << "/help - Show this help" << std::endl;
    std::cout << "/file <filename> - Send file to all users" << std::endl;
    std::cout << "/sendto <username> <filename> - Send file to specific user" << std::endl;
    std::cout << "/msg <username> <message> - Send private message" << std::endl;
    std::cout << "/users - List online users" << std::endl;
    std::cout << "/quit - Exit chat" << std::endl;
    std::cout << "================\n" << std::endl;
}

// Driver function
int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " <server_ip> <port> <username>" << std::endl;
        std::cout << "Example: " << argv[0] << " 127.0.0.1 8888 Alice" << std::endl;
        return 1;
    }
    
    std::string server_ip = argv[1];
    int port = std::stoi(argv[2]);
    username = argv[3];
    
    if (!init_sockets()) {
        std::cerr << "Failed to initialize sockets" << std::endl;
        return 1;
    }
    
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        cleanup_sockets();
        return 1;
    }
    
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Convert IP address (Windows compatible)
    server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
    if (server_addr.sin_addr.s_addr == INADDR_NONE) {
        std::cerr << "Invalid address" << std::endl;
        CLOSE_SOCKET(client_socket);
        cleanup_sockets();
        return 1;
    }
    
    std::cout << "Connecting to " << server_ip << ":" << port << "..." << std::endl;
    
    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        CLOSE_SOCKET(client_socket);
        cleanup_sockets();
        return 1;
    }
    
    // Send username to server
    send(client_socket, username.c_str(), static_cast<int>(username.length()), 0);
    
    std::cout << "Connected to chat server as '" << username << "'" << std::endl;
    std::cout << "Type /help for commands." << std::endl;
    std::cout << "> " << std::flush;
    
    std::thread receiver(receive_messages);
    
    std::string input;
    while (running) {
        if (KEYBOARD_HIT()) {
            char c = GET_CHAR();
            
#ifdef _WIN32
            if (c == KEY_ENTER) {
#else
            if (c == KEY_ENTER || c == '\r') {
#endif
                if (!input.empty()) {
                    if (input == "/quit") {
                        std::cout << "\nExiting..." << std::endl;
                        running = false;
                        break;
                    }
                    else if (input == "/help") {
                        print_help();
                    }
                    else if (input == "/users") {
                        int msg_type = MSG_CHAT;
                        send(client_socket, reinterpret_cast<char*>(&msg_type), sizeof(int), 0);
                        std::string req = "/users";
                        send(client_socket, req.c_str(), static_cast<int>(req.length()), 0);
                    }
                    else if (input.substr(0, 5) == "/file") {
                        if (input.length() > 6) {
                            std::string filename = input.substr(6);
                            send_file(filename);
                        } else {
                            std::cout << "Usage: /file <filename>" << std::endl;
                        }
                    }
                    else if (input.substr(0, 7) == "/sendto") {
                        // Format: /sendto username filename
                        size_t first_space = input.find(' ', 8);
                        if (first_space != std::string::npos) {
                            std::string target = input.substr(8, first_space - 8);
                            std::string filename = input.substr(first_space + 1);
                            send_file(filename, target);
                        } else {
                            std::cout << "Usage: /sendto <username> <filename>" << std::endl;
                        }
                    }
                    else if (input.substr(0, 4) == "/msg") {
                        // Format: /msg username message
                        size_t first_space = input.find(' ', 5);
                        if (first_space != std::string::npos) {
                            std::string target = input.substr(5, first_space - 5);
                            std::string message = input.substr(first_space + 1);
                            
                            int msg_type = MSG_PRIVATE;
                            send(client_socket, reinterpret_cast<char*>(&msg_type), sizeof(int), 0);
                            
                            std::string private_msg = target + ":" + message;
                            send(client_socket, private_msg.c_str(), static_cast<int>(private_msg.length()), 0);
                            
                            std::cout << "[Private message sent to " << target << "]" << std::endl;
                        } else {
                            std::cout << "Usage: /msg <username> <message>" << std::endl;
                        }
                    }
                    else {
                        // Regular chat message
                        int msg_type = MSG_CHAT;
                        send(client_socket, reinterpret_cast<char*>(&msg_type), sizeof(int), 0);
                        send(client_socket, input.c_str(), static_cast<int>(input.length()), 0);
                    }
                    
                    input.clear();
                    std::cout << "> " << std::flush;
                }
            }
#ifdef _WIN32
            else if (c == KEY_BACKSPACE) {
#else
            else if (c == KEY_BACKSPACE || c == 8) {
#endif
                if (!input.empty()) {
                    input.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            }
            else {
                input += c;
                std::cout << c << std::flush;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Clean shutdown
    if (receiver.joinable()) {
        // Detach instead of join to avoid blocking
        receiver.detach(); 
    }
    
#ifdef _WIN32
    shutdown(client_socket, SD_BOTH);
#else
    shutdown(client_socket, SHUT_RDWR);
#endif
    
    CLOSE_SOCKET(client_socket);
    cleanup_sockets();
    
    return 0;
}