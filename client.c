#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <pthread.h>
#include <errno.h>

#include "Jindef.h"

#define SOCKET_ERROR -1
#define RECV_INTERVAL_MILLISECONDS 200

/**
 * サーバーから来る命令内容
 */
typedef enum
{
    MAP_DATA, // マップデータ受信
    END_GAME // ゲーム終了
} Command;

/**
 * サーバーからくる初期データ
 */
typedef struct
{
    char color;
    char x;
    char y;
} StartData;

/**
 * サーバーに自分が進んだ方向を送る
 * @param sock サーバーのソケット
 * @param color 自分の色
 * @param dir 進んだ方向
 */
void send_move(int sock, Color color, Direction dir)
{
    char message[2] = {color, dir};
    write(sock, message, sizeof(message));
}

/**
 * サーバーから初期データを受け取る
 * @param sock サーバーのソケット
 * @param output_data 初期データを受け取る構造体ポインタ
 */
void recv_start_data(int sock, StartData* output_data)
{
    read(sock, &(output_data->color), 1);
    output_data->color++;
    read(sock, &(output_data->x), 1);
    read(sock, &(output_data->y), 1);
}

/**
 * ncursesを初期化する
 */
void init_ncurses()
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    start_color();
    init_pair(1, COLOR_WHITE, COLOR_RED);
    init_pair(2, COLOR_WHITE, COLOR_GREEN);
    init_pair(3, COLOR_WHITE, COLOR_YELLOW);
    init_pair(4, COLOR_WHITE, COLOR_BLUE);
    init_pair(5, COLOR_WHITE, COLOR_CYAN);
}

/**
 * ncursesの終了処理をする
 */
void exit_ncurses()
{
    endwin();
}

/**
 * マップの1マスを描画する
 * @param tile そのマスの色情報
 * @param x X座標
 * @param y Y座標
 */
void draw_tile(Color tile, int x, int y)
{
    int draw_color_pair;
    char draw_chars[] = {' ', ' ', '@'};
    char draw_char;
    int index = tile > 0 ? (int)((tile - 1) / 4) : 0;

    draw_color_pair = COLOR_PAIR(tile > 0 ? (tile - 1) % 4 + 1 : 5);
    draw_char = draw_chars[index];

    attron(draw_color_pair);
    mvprintw(y, x, "%c", draw_char);
    attroff(draw_color_pair);
}

/**
 * 陣取りマップを描画する
 * @param map マップデータ
 */
void draw_map(const char* map)
{
    int i, j;
    Color tile;

    for (i = 0; i < MAP_HEIGHT; i++) {
        for (j = 0; j < MAP_WIDTH; j++) {
            tile = map[i*MAP_WIDTH + j];
            draw_tile(tile, j, i);
        }
    }
}

Direction input_dir()
{
    switch (getch()) {
        case KEY_DOWN:
        case 'j':
        case 's':
            return DOWN;

        case KEY_LEFT:
        case 'h':
        case 'a':
            return LEFT;

        case KEY_UP:
        case 'w':
        case 'k':
            return UP;

        case KEY_RIGHT:
        case 'd':
        case 'l':
            return RIGHT;

        default:
            return UNKNOWN;
    }
}


/**
 * 指定したソケットから確実にlengthバイト分readする
 * @return 途中で失敗した場合は0を返す
 * 成功したら1を返す
 */
int recv_complete_nonblocking(int sock, char* buf, int length)
{
    int total_read_count = 0;
    int recv_result;

    while (total_read_count < length) {
        recv_result = read(sock, buf + total_read_count, length - total_read_count);
        if (recv_result < 1) {
            if (errno == EAGAIN) {
                refresh();
                usleep(RECV_INTERVAL_MILLISECONDS * 1000);
            }
            else {
                return 0;
            }
            total_read_count += recv_result;
        }
    }
    return 1;
}

/**
 * 指定したソケットから確実にlengthバイト分readする
 * @return 途中で失敗した場合は0を返す
 * 成功したら1を返す
 */
int recv_complete(int sock, char* buf, int length)
{
    int total_read_count = 0;
    int recv_result;

    while (total_read_count < length) {
        recv_result = read(sock, buf + total_read_count, length - total_read_count);
        if (recv_result < 1) {
                return 0;
        }
        total_read_count += recv_result;
        refresh();
    }
    return 1;
}

/**
 * 別スレッドで実行される，マップ描画関数
 */
void draw_map_thread(int* sock)
{
    char map_data[MAP_WIDTH * MAP_HEIGHT];
    int val = 1;
    int recv_result;

    while (1) {
        recv_result = recv_complete(*sock, map_data, MAP_WIDTH * MAP_HEIGHT);
        if (recv_result != 1) {
            perror("recv_complete_nonblocking");
            break;
        }
        draw_map(map_data);
    }
}

/**
 * ゲームを始める
 */
void start_game(int sock, const StartData* start_data)
{
    Color color = start_data->color;
    Direction dir;
    int x = start_data->x;
    int y = start_data->y;
    pthread_t draw_thread_id;

    init_ncurses();

    pthread_create(&draw_thread_id, NULL, (void*)draw_map_thread, &sock);

    while (1) {
        dir = input_dir();
        if (dir != UNKNOWN) {
            send_move(sock, color, dir);
        }
    } 

    exit_ncurses();
}

/**
 * サーバーに接続する
 * @return サーバーとの接続ソケット
 */
int connect_server(const char* ip_addr, unsigned short server_port)
{
    int result;
    int sock;
    struct sockaddr_in addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("create socket");
        return SOCKET_ERROR;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    addr.sin_addr.s_addr = inet_addr(ip_addr);

    // サーバーに接続する
    result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR) {
        perror("connect");
        exit(1);
    }
    return sock;
}

/**
 * メイン関数
 */
int main(int argc, char* argv[])
{
    int sock;
    int forked_pid;
    char command;
    StartData start_data;

    if (argc != 3) {
        printf("引数の数が不正です！\n");
        printf("使い方: ./client.out <ip_address> <port>\n");
        printf("例）./client.out 127.0.0.1 10800\n");
        return 1;
    }

    // サーバーに接続する
    sock = connect_server(argv[1], atoi(argv[2]));
    
    // サーバーから初期データを受け取る
    recv_start_data(sock, &start_data);
    printf("received start_data\n");
    printf("your color: %d\n", start_data.color);
    printf("your position: (%d, %d)\n", start_data.x, start_data.y);
    
    // ゲームスタート
    start_game(sock, &start_data);

    /*
    // forkしてキー入力プロセスとネットワークプロセスに分離する
    forked_pid = fork();
    if (forked_pid == 0) {
        // 子プロセスの場合，時間表示プロセス
        // TODO
    }
    else if (forked_pid > 0) {
        // 親プロセスの場合, ゲームプロセス
        start_game(sock, &start_data);
    }
    */

    close(sock);

    return 0;
}
