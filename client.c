#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 255
#define MAX_MAP_WIDTH 50
#define MAX_MAP_HEIGHT 50

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

int main(void)
{
    int i, k;
    int sock;
    struct sockaddr_in addr;
    MapData data;

    // create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("create socket");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(10800);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    recv_map_data(sock, &data);
    printf("command: %d\n", data.command);
    printf("width: %d\n", data.map_width);
    printf("height: %d\n", data.map_height);
    for (i = 0; i < data.map_height; i++) {
        for (k = 0; k < data.map_width; k++) {
            printf("%d ", data.map_data[i*data.map_width + k]);
        }
        printf("\n");
    }

    close(sock);

    return 0;
}
