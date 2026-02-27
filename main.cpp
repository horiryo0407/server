#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
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

    struct Quiz {
        std::string question;
        std::string answer;
    };

    void broadcast(const std::vector<Player>& players, const std::string& text) {
        for (const auto& p : players) {
            if (p.connected && p.socket != INVALID_SOCKET) {
                send(p.socket, text.c_str(), (int)text.size(), 0);
            }
        }
    }

    std::string toHiragana(const std::string& input)
    {
        // マルチバイト → UTF-16
        int wlen = MultiByteToWideChar(
            CP_ACP, 0,
            input.c_str(), -1,
            nullptr, 0);

        if (wlen == 0) return input;

        std::wstring wstr(wlen, L'\0');

        MultiByteToWideChar(
            CP_ACP, 0,
            input.c_str(), -1,
            &wstr[0], wlen);

        // カタカナ → ひらがな変換
        for (auto& ch : wstr) {
            if (ch >= L'ァ' && ch <= L'ヶ') {
                ch -= 0x60;
            }
        }

        // UTF-16 → マルチバイト
        int mlen = WideCharToMultiByte(
            CP_ACP, 0,
            wstr.c_str(), -1,
            nullptr, 0,
            nullptr, nullptr);

        std::string result(mlen, '\0');

        WideCharToMultiByte(
            CP_ACP, 0,
            wstr.c_str(), -1,
            &result[0], mlen,
            nullptr, nullptr);

        result.pop_back(); // null削除
        return result;
    }

    void trimNewline(std::string& s) 
    {
        size_t pos = s.find_last_not_of("\r\n");
        if (pos != std::string::npos)
            s.erase(pos + 1);
        else
            s.clear();
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

    std::cout << "=== クイズサーバー起動 ===\n";

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
    }
    for (int i = 3; i >= 1; --i) {
        broadcast(players, "\n=== " + std::to_string(i) + " ===\n");
        Sleep(1000);
    }

    broadcast(players, "\n=== ゲーム開始！ ===\n");
    broadcast(players, "\n=== スタート！ ===\n");

    std::vector<Quiz> quizzes = {
        { "ポケットモンスターに登場する大人気のでんきねずみポケモンといえば？", "ピカチュウ" },
        { "ドラえもんの好物は？", "どらやき" },
        { "スーパーマリオの主人公の名前は？", "マリオ" },
		{ "ワンピースの主人公の名前は？", "ルフィ" },
		{ "ジブリ映画『となりのトトロ』で、サツキとメイが出会う不思議な生き物は？", "トトロ" },
		{ "日本の国鳥は？", "キジ" },
		{ "GE2Aの担任の苗字は？", "こまむら" },
		{ "11*11は？", "121" }
    };

    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(quizzes.begin(), quizzes.end(), rng);

    for (const auto& quiz : quizzes) {

        broadcast(players, "\n>> " + quiz.question + "\n");

        bool round_over = false;
        std::vector<std::string> lastAnswer(players.size());

        while (!round_over) {

            for (size_t i = 0; i < players.size(); ++i) {

                auto& p = players[i];

                char buf[256] = {};
                int ret = recv(p.socket, buf, sizeof(buf) - 1, 0);

                if (ret > 0) {
                    buf[ret] = '\0';

                    std::string answer(buf);
                    trimNewline(answer);

                    // 同じ入力なら無視（連続減点防止）
                    if (answer == lastAnswer[i])
                        continue;

                    lastAnswer[i] = answer;

                    std::string normAnswer = toHiragana(answer);
                    std::string normCorrect = toHiragana(quiz.answer);

                    if (normAnswer == normCorrect) {

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
                    else {

                        if (p.score > 0)
                            p.score--;

                        broadcast(players,
                            "プレイヤー" + std::to_string(p.id) +
                            " がミス！ -1点\n");

                        broadcast(players,
                            "スコア： P1[" +
                            std::to_string(players[0].score) +
                            "] P2[" +
                            std::to_string(players[1].score) + "]\n");
                    }
                }

                Sleep(10);
            }

            if (players[0].score >= 5 || players[1].score >= 5)
                break;
        }
        // 勝利点に達したら全体ループも抜ける
        if (players[0].score >= 5 || players[1].score >= 5)
            break;
    }
    // ゲームループ終了後

    int winner = 0;

    if (players[0].score > players[1].score)
        winner = 1;
    else if (players[1].score > players[0].score)
        winner = 2;

    broadcast(players, "\n=== ゲーム終了 ===\n");

    if (winner == 1)
        broadcast(players, "プレイヤー1の勝利！\n");
    else if (winner == 2)
        broadcast(players, "プレイヤー2の勝利！\n");
    else
        broadcast(players, "引き分け！\n");

        for (auto& p : players)
            closesocket(p.socket);

        closesocket(listen_sock);
        WSACleanup();
        return 0;
}