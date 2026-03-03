#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
    typedef int socklen_t;
    
    // Windows doesn't have this by default
    #ifndef INET_ADDRSTRLEN
    #define INET_ADDRSTRLEN 16
    #endif
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/select.h>
    #define CLOSE_SOCKET close
    #define SOCKET int
    #define INVALID_SOCKET -1
#endif

const int PORT = 8888;
const int MAX_CLIENTS = 10;
const int BUFFER_SIZE = 4096;
const int FILE_BUFFER_SIZE = 8192;

std::mutex clients_mutex;
std::vector<SOCKET> clients;
std::vector<std::string> client_names;

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
    char target[50];  // "all" for broadcast, or specific username
    unsigned long long filesize;
    int total_chunks;
    int current_chunk;
};

// Windows-compatible inet_ntop replacement
const char* win_inet_ntop(int af, const void* src, char* dst, socklen_t size) {
#ifdef _WIN32
    struct sockaddr_in src_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = af;
    memcpy(&(src_addr.sin_addr), src, sizeof(struct in_addr));
    
    DWORD dst_size = size;
    if (WSAAddressToStringA((struct sockaddr*)&src_addr, sizeof(src_addr), NULL, dst, &dst_size) == 0) {
        return dst;
    }
    return NULL;
#else
    return inet_ntop(af, src, dst, size);
#endif
}

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

void broadcast_message(const std::string& message, SOCKET sender, bool include_sender = false) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    for (SOCKET client : clients) {
        if (include_sender || client != sender) {
            send(client, message.c_str(), static_cast<int>(message.length()), 0);
        }
    }
}

void send_private_message(const std::string& message, const std::string& target_name) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    for (size_t i = 0; i < clients.size(); i++) {
        if (client_names[i] == target_name) {
            send(clients[i], message.c_str(), static_cast<int>(message.length()), 0);
            break;
        }
    }
}

std::string get_client_name(SOCKET client) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] == client) {
            return client_names[i];
        }
    }
    return "Unknown";
}

void handle_file_transfer(SOCKET client, const FileMetadata& meta, const char* data, int data_len) {
    static std::map<std::pair<std::string, std::string>, std::ofstream> file_streams;
    static std::map<std::pair<std::string, std::string>, unsigned long long> bytes_received;
    
    std::string sender_name = get_client_name(client);
    std::string target = meta.target;
    std::string filename = meta.filename;
    auto key = std::make_pair(sender_name, filename);
    
    if (meta.current_chunk == 0) {
        // First chunk - open file
        std::string save_path = "received_" + filename;
        file_streams[key].open(save_path, std::ios::binary);
        bytes_received[key] = 0;
        
        std::cout << "Receiving file: " << filename << " from " << sender_name 
                  << " (" << meta.filesize << " bytes)" << std::endl;
    }
    
    if (file_streams[key].is_open()) {
        file_streams[key].write(data, data_len);
        bytes_received[key] += data_len;
        
        if (bytes_received[key] >= meta.filesize) {
            file_streams[key].close();
            file_streams.erase(key);
            bytes_received.erase(key);
            
            std::cout << "File received completely: " << filename << std::endl;
            
            // Send ACK to sender
            std::string ack = "File '" + std::string(meta.filename) + "' received successfully";
            send(client, ack.c_str(), static_cast<int>(ack.length()), 0);
            
            // Notify other clients if broadcast
            if (std::string(meta.target) == "all") {
                std::string notification = "[SERVER] " + sender_name + " shared file: " + filename;
                broadcast_message(notification, client);
            }
        }
    }
}

void handle_client(SOCKET client_socket) {
    char buffer[FILE_BUFFER_SIZE];
    std::string client_name;
    
    // Receive client name
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        CLOSE_SOCKET(client_socket);
        return;
    }
    
    buffer[bytes_received] = '\0';
    client_name = buffer;
    
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back(client_socket);
        client_names.push_back(client_name);
    }
    
    std::string welcome_msg = "[SERVER] " + client_name + " joined the chat!";
    std::cout << welcome_msg << std::endl;
    broadcast_message(welcome_msg, client_socket);
    
    // Send list of online users
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        std::string user_list = "[SERVER] Online users: ";
        for (const auto& name : client_names) {
            user_list += name + " ";
        }
        send(client_socket, user_list.c_str(), static_cast<int>(user_list.length()), 0);
    }
    
    while (true) {
        // First receive message type
        int msg_type;
        bytes_received = recv(client_socket, reinterpret_cast<char*>(&msg_type), sizeof(int), 0);
        
        if (bytes_received <= 0) {
            break;
        }
        
        if (msg_type == MSG_CHAT || msg_type == MSG_PRIVATE) {
            // Receive chat message
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) break;
            
            buffer[bytes_received] = '\0';
            std::string message = buffer;
            
            if (msg_type == MSG_CHAT) {
                std::string formatted_msg = client_name + ": " + message;
                std::cout << formatted_msg << std::endl;
                broadcast_message(formatted_msg, client_socket);
            } else {
                // Private message format: "target:message"
                size_t colon_pos = message.find(':');
                if (colon_pos != std::string::npos) {
                    std::string target = message.substr(0, colon_pos);
                    std::string content = message.substr(colon_pos + 1);
                    std::string private_msg = "[PRIVATE from " + client_name + "] " + content;
                    send_private_message(private_msg, target);
                }
            }
        }
        else if (msg_type == MSG_FILE_META) {
            // Receive file metadata
            FileMetadata meta;
            bytes_received = recv(client_socket, reinterpret_cast<char*>(&meta), sizeof(FileMetadata), 0);
            if (bytes_received <= 0) break;
            
            // Start receiving file data
            int total_chunks = meta.total_chunks;
            for (int i = 0; i < total_chunks; i++) {
                bytes_received = recv(client_socket, buffer, FILE_BUFFER_SIZE, 0);
                if (bytes_received <= 0) break;
                
                handle_file_transfer(client_socket, meta, buffer, bytes_received);
            }
        }
    }
    
    // Client disconnected
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = std::find(clients.begin(), clients.end(), client_socket);
        if (it != clients.end()) {
            int index = it - clients.begin();
            client_name = client_names[index];
            clients.erase(it);
            client_names.erase(client_names.begin() + index);
        }
    }
    
    std::string leave_msg = "[SERVER] " + client_name + " left the chat.";
    std::cout << leave_msg << std::endl;
    broadcast_message(leave_msg, client_socket);
    
    CLOSE_SOCKET(client_socket);
}

int main() {
    if (!init_sockets()) {
        std::cerr << "Failed to initialize sockets" << std::endl;
        return 1;
    }
    
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        cleanup_sockets();
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        CLOSE_SOCKET(server_socket);
        cleanup_sockets();
        return 1;
    }
    
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        std::cerr << "Listen failed" << std::endl;
        CLOSE_SOCKET(server_socket);
        cleanup_sockets();
        return 1;
    }
    
    std::cout << "Chat server started on port " << PORT << std::endl;
    std::cout << "Waiting for connections..." << std::endl;
    
    std::vector<std::thread> client_threads;
    
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }
        
        // Get client IP address (Windows compatible)
        char client_ip[INET_ADDRSTRLEN];
        win_inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        
        std::cout << "New connection from: " << client_ip << std::endl;
        
        client_threads.emplace_back(handle_client, client_socket);
        client_threads.back().detach();
    }
    
    CLOSE_SOCKET(server_socket);
    cleanup_sockets();
    
    return 0;
}