#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <algorithm>
#include <random>

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
                send(p.socket, text.c_str(), (int)text.size(), 0);
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

    bind(listen_sock, (SOCKADDR*)&addr, sizeof(addr));
    listen(listen_sock, SOMAXCONN);

    std::cout << "=== クイズサーバー起動 ===\n2人の接続を待っています...\n";

    std::vector<Player> players(2);

    for (int i = 0; i < 2; ++i) {
        players[i].socket = accept(listen_sock, nullptr, nullptr);

        u_long nonblock = 1;
        ioctlsocket(players[i].socket, FIONBIO, &nonblock);

        players[i].id = i + 1;
        players[i].connected = true;

        std::string msg =
            "ようこそ！あなたは プレイヤー" +
            std::to_string(players[i].id) + " です\n";
        send(players[i].socket, msg.c_str(), (int)msg.size(), 0);

        std::cout << "プレイヤー" << players[i].id << " が接続しました\n";
    }

    broadcast(players, "\n2人そろいました！\n");
    for (int i = 3; i > 0; --i) {
        broadcast(players, std::to_string(i) + "...\n");
        Sleep(1000);
    }
    broadcast(players, "=== ゲーム開始！！ ===\n");

    // 問題リスト（24問）
    std::vector<std::string> questions = {
        "リーチ", "イッパツ", "メンゼンチンツモホウ", "ピンフ",
        "イーペーコー", "サンショクドウジュン", "イッキツウカン", "トイトイホー", "サンアンコ",
        "ショウサンゲン","ホンイーソー","チンイーソー","コクシムソウ","スーアンコウ","ダイサンゲン",
        "ツーイーソー","リューイーソー","チューレンポウトウ","テンホウ","チーホウ"
        "メッシ", "クリスティアーノロナウド","ジョン",
    };

    // シャッフルして被り防止
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(questions.begin(), questions.end(), rng);
    size_t questionIndex = 0;

    while (true) {
        Sleep(2000);

        if (questionIndex >= questions.size()) {
            broadcast(players, "\n問題がなくなりました！\n");
            break;
        }

        std::string question = questions[questionIndex++];

        broadcast(players,
            "\n>> 次の文字を入力してください： [ " +
            question + " ]\n");

        bool round_over = false;

        while (!round_over) {
            for (auto& p : players) {
                if (!p.connected) continue;

                char buf[256] = {};
                int ret = recv(p.socket, buf, sizeof(buf) - 1, 0);

                if (ret > 0) {
                    buf[ret] = '\0';

                    std::string answer(buf);
                    answer.erase(answer.find_last_not_of("\r\n") + 1);

                    if (answer == question) {
                        p.score++;

                        broadcast(players,
                            "プレイヤー" + std::to_string(p.id) +
                            " が正解！ +1点\n");

                        broadcast(players,
                            "スコア： P1[" +
                            std::to_string(players[0].score) +
                            "] P2[" +
                            std::to_string(players[1].score) + "]\n");

                        round_over = true;
                        break;
                    }
                }
            }
            Sleep(10);
        }

        if (players[0].score >= 5 || players[1].score >= 5)
            break;
    }

    // 勝敗表示
    if (players[0].score > players[1].score) {
        broadcast(players, "\n=== プレイヤー1の勝利かもしれない ===\n");
    }
    else if (players[1].score > players[0].score) {
        broadcast(players, "\n=== プレイヤー2の勝利なんだって ===\n");
    }
    else {
        broadcast(players, "\n=== 引き分けにはならないでしょ！ ===\n");
    }

    broadcast(players, "ゲーム終了だって！お疲れさまー\n");

    for (auto& p : players) {
        if (p.socket != INVALID_SOCKET)
            closesocket(p.socket);
    }

    closesocket(listen_sock);
    WSACleanup();
    return 0;
}