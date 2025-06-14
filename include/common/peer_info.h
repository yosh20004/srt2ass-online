#ifndef PEER_INFO_H
#define PEER_INFO_H

#include <string>
#include <optional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct PeerInfo {
    std::string ip_address;
    unsigned int port;
};

inline std::optional<PeerInfo> get_peer_info(int socket_fd) {
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    
    if (getpeername(socket_fd, (struct sockaddr *)&addr, &addr_size) == -1) {
        return std::nullopt;
    }
    
    PeerInfo info;
    info.ip_address = inet_ntoa(addr.sin_addr);
    info.port = ntohs(addr.sin_port);
    
    return info;
}

#endif 