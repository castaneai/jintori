#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 255
#define MAX_MAP_WIDTH 50
#define MAX_MAP_HEIGHT 50
#define INTERNAL_PORT 10801
#define SOCKET_ERROR -1

/**
 * 色を表す列挙体
 */
typedef enum
{
    RED,
    GREEN,
    BLUE
} Color;

/**
 * 方向を表す列挙体
 */
typedef enum
{
    LEFT,
    UP,
    RIGHT,
    DOWN
} Direction;

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
 * サーバーから来るマップデータ
 */
typedef struct
{
    char command;
    char map_width;
    char map_height;
    char map_data[MAX_MAP_HEIGHT * MAX_MAP_WIDTH];
} MapData;

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
    read(sock, &(output_data->x), 1);
    read(sock, &(output_data->y), 1);
}

/**
 * サーバーからマップデータを受け取る
 * @param sock サーバーのソケット
 * @param output_data マップデータを受け取る構造体ポインタ
 */
void recv_map_data(int sock, MapData* output_data)
{
    read(sock, &(output_data->command), 1);
    read(sock, &(output_data->map_width), 1);
    read(sock, &(output_data->map_height), 1);
    read(sock, &(output_data->map_data), output_data->map_width * output_data->map_height);
}

/**
 * ソケットに指定したポートをBINDする
 */
void bind_socket(int sock, unsigned short port)
{
    int result;
    struct sockaddr_in server_addr;
    int server_addr_len;
    int socket_opt_flag = 1;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    server_addr_len = sizeof(server_addr);

    result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
            &socket_opt_flag, sizeof(socket_opt_flag));
    if (result == SOCKET_ERROR) {
        perror("setsockopt");
        exit(1);
    }

    result = bind(sock, (struct sockaddr*)&server_addr, server_addr_len);
    if (result == SOCKET_ERROR) {
        perror("bind");
        exit(1);
    }
}

void start_input_process()
{
}

int main(void)
{
    int i, k;
    int sock;
    int forked_pid;
    struct sockaddr_in addr;
    StartData start_data;
    MapData map_data;

    // create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("create socket");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(10800);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // サーバーに接続する
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    // サーバーから初期データを受け取る
    recv_start_data(sock, &start_data);

    // forkしてキー入力プロセスとネットワークプロセスに分離する
    forked_pid = fork();
    if (forked_pid == 0) {
        // 子プロセスの場合，キー入力(ncurses)プロセス
        start_input_process();
        exit(0);
    }
    else if (forked_pid > 0) {
        // 親プロセスの場合, 
    }

    close(sock);

    return 0;
}
