#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>

#pragma comment(lib, "Ws2_32.lib")

namespace {
    struct Player {
        SOCKET socket = INVALID_SOCKET;
        int id = 0;
        int score = 0;
        bool connected = false;
    };

    void broadcast(const std::vector<Player>& players, const std::string& text) {
        for (const auto& p : players) {
            if (p.connected && p.socket != INVALID_SOCKET) {
                send(p.socket, text.c_str(), static_cast<int>(text.size()), 0);
            }
        }
    }
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    SOCKADDR_IN addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(50000);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_sock, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr));
    listen(listen_sock, SOMAXCONN);

    std::cout << "--- QUIZ SERVER STARTED ---\nWaiting for 2 players...\n";

    std::vector<Player> players(2);
    for (int i = 0; i < 2; ++i) {
        players[i].socket = accept(listen_sock, nullptr, nullptr);
        u_long flag = 1;
        ioctlsocket(players[i].socket, FIONBIO, &flag);
        players[i].id = i + 1;
        players[i].connected = true;
        std::string welcome = "WELCOME! You are Player " + std::to_string(players[i].id) + "\n";
        send(players[i].socket, welcome.c_str(), (int)welcome.size(), 0);
        std::cout << "Player " << players[i].id << " joined.\n";
    }

    broadcast(players, "\nBoth players connected! Ready?\n");
    for (int i = 3; i > 0; --i) {
        broadcast(players, "Starting in " + std::to_string(i) + "...\n");
        Sleep(1000);
    }
    broadcast(players, "--- GAME START!! ---\n");

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    while (true) {
        Sleep(2000);
        int question_id = std::rand() % 9000 + 1000;
        broadcast(players, "\n>> TYPE THIS: [ " + std::to_string(question_id) + " ]\n");

        bool round_over = false;
        while (!round_over) {
            for (auto& player : players) {
                if (!player.connected) continue;
                char buf[256] = {};
                int ret = recv(player.socket, buf, sizeof(buf) - 1, 0);

                if (ret > 0) {
                    buf[ret] = '\0';
                    if (std::atoi(buf) == question_id) {
                        player.score++;
                        broadcast(players, "Player " + std::to_string(player.id) + " point!\n");
                        broadcast(players, "SCORE: P1[" + std::to_string(players[0].score) + "] P2[" + std::to_string(players[1].score) + "]\n");
                        round_over = true;
                        break;
                    }
                }
                else if (ret == 0) { // ê≥èÌêÿíf
                    std::cout << "Player " << player.id << " quit.\n";
                    player.connected = false;
                    broadcast(players, "\nPLAYER " + std::to_string(player.id) + " LEFT. GAME OVER.\n");
                    goto end_game;
                }
                else { // àŸèÌÇ‹ÇΩÇÕÉfÅ[É^Ç»Çµ
                    int err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK) {
                        std::cout << "Player " << player.id << " connection lost.\n";
                        player.connected = false;
                        broadcast(players, "\nCONNECTION LOST WITH PLAYER " + std::to_string(player.id) + "\n");
                        goto end_game;
                    }
                }
            }
            Sleep(10);
        }

        if (players[0].score >= 5 || players[1].score >= 5) break;
    }

end_game:
    std::cout << "Cleaning up...\n";
    for (auto& p : players) {
        if (p.socket != INVALID_SOCKET) closesocket(p.socket);
    }
    closesocket(listen_sock);
    WSACleanup();
    std::cout << "Server shutdown.\n";
    return 0;
}