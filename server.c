/**
 * UNIXソケットサーバー
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "Jindef.h"

#define BUFSIZE 80 // 送受信データのバッファサイズ
#define ERROR -1 // ソケット関連のエラーを表す値
#define FORKED_CHILD 0 // fork()された子プロセスであることを示す値
#define DEFAULT_LISTEN_BACKLOG 5 // listenする際の標準の受入数


/*
 * Mapに値をセットする
 */
void setMap(unsigned char map[MAP_HEIGHT][MAP_WIDTH], int color, int x, int y){
	map[y][x] = color;
}	

/*
 * デバッグ用のマップ出力関数
 * */
void printMap(unsigned char map[MAP_HEIGHT][MAP_WIDTH]){
	int i, j;
	char *colorMap = "rgbyRGBY@^!]";
    int a,b;
	for(i = 0;i < MAP_HEIGHT;i++){
		for(j = 0;j < MAP_WIDTH;j++){
			a = (map[i][j] - 1) % PLAYER_MAX;
            b = ((map[i][j] - 1) / PLAYER_MAX) * PLAYER_MAX;
            printf("%c", (map[i][j] != 0)? colorMap[a+b]:'.');
		}
		printf("\n");
	}
}

/*
 * 移動用関数
 * */
void move(unsigned char map[MAP_HEIGHT][MAP_WIDTH], int color, Position *pos, Direction direction){
	unsigned char to;
	map[pos->y][pos->x] = color + PLAYER_MAX + 1;
	switch (direction){
		case LEFT:
		   	to = map[pos->y][pos->x - 1];
			if(pos->x > 0 && (to < PLAYER_MAX*2+1)) pos->x--;
			break;
		case UP:
		   	to = map[pos->y - 1][pos->x];
			if(pos->y > 0 && (to < PLAYER_MAX*2+1)) pos->y--;
			break;
		case RIGHT:
		   	to = map[pos->y][pos->x + 1];
			if(pos->x < MAP_WIDTH-1 && (to < PLAYER_MAX*2+1)) pos->x++;
			break;
		case DOWN:
		   	to = map[pos->y + 1][pos->x];
			if(pos->y < MAP_HEIGHT - 1 && (to < PLAYER_MAX*2+1)) pos->y++;
			break;
        default:
            break;
	}
	map[pos->y][pos->x] = color + PLAYER_MAX*2 + 1;
}

/**
 * エラーが起こった時共通して呼び出す関数
 */
void error(const char* message)
{
    perror(message);
    exit(1);
}

/**
 * 指定したソケットに文字を書き込み，最後に改行を入れる
 */
int write_line(int socket, const char* message)
{
    int result = 0;
    result += write(socket, message, strlen(message));
    result += write(socket, "\r\n", 2);
    return result;
}

/**
 * 接続してきたひとりのクライアントを処理する
 * @param client_socket 接続してきたクライアントのソケット
 * @param client_address 接続してきたクライアントのアドレス
 */
void process_client(int client_socket, struct sockaddr_in* client_address)
{
    write_line(client_socket, "HTTP/1.0 200 OK");
    write_line(client_socket, "text/html");
    write_line(client_socket, "\r\n");
    write_line(client_socket, "やっはろー");
}

/**
 * ソケットに指定されたポート番号をBINDする
 */
void bind_socket(int socket, unsigned short port)
{
    int result;
    struct sockaddr_in server_address;
    int server_address_length;
    int socket_option_flag = 1;

    // BINDする受け入れアドレスを設定する
    server_address.sin_family = AF_INET;
    // INADDR_ANY はすべての外部接続を受け入れるアドレス指定
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);
    server_address_length = sizeof(server_address);

    // サーバーをすぐに再起動してもポート番号を再利用可能にする
    result = setsockopt(
            socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &socket_option_flag,
            sizeof(socket_option_flag));
    if (result == ERROR) {
        error("setsockopt");
    }

    // bindを実行
    result = bind(socket, (struct sockaddr*)&server_address, server_address_length);
    if (result == ERROR) {
        error("bind");
    }
}

/**
 * サーバーを開始する
 * @param port 待ち受けポート番号
 */
void start_server(unsigned short port)
{
    int result;
    int server_socket;
	int server_local_socket;
    int client_sockets[PLAYER_MAX];
	char client_buf[PLAYER_MAX][10];
	int client_recv_len[PLAYER_MAX];
    struct sockaddr_in client_address;
    unsigned int client_address_length;
    int forked_pid;
	char player_num = 0;
	int i, j, k;
	int mode = 1;
	int temp = 0;
	Position player_position[] = {{0,0},{10,0},{0,10},{10,10},{5,5}};
	unsigned char map[MAP_HEIGHT][MAP_WIDTH];
	int debug = 0;
	struct timeval time;
	time_t start_sec;
    long before_map_send;
    

	//マップを初期化する
	for(i = 0;i < MAP_HEIGHT;i++){
		for(j = 0;j < MAP_WIDTH;j++){
			map[i][j] = 0x0;
			for(k = 0;k < PLAYER_MAX;k++){
				if(player_position[k].x == j && player_position[k].y == i){
					map[i][j] = k + PLAYER_MAX * 2 + 1;
				}	
			}
		}
	}
	//recv_lenを初期化する
	for(i = 0;i < PLAYER_MAX;i++){
		client_recv_len[i] = 0;
	}

    // 受け入れ用のサーバソケットを作る
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == ERROR) {
        error("create server_socket");
    }

	//子プロセスとの通信ようソケット
	server_local_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_local_socket == ERROR){
		error("create server_local_socket");
	}

    bind_socket(server_socket, port);
	printf("bind to port: %d ok.\n", port);

	result = listen(server_socket, DEFAULT_LISTEN_BACKLOG);
	if (result == ERROR) {
		close(server_socket);
		error("listen");
	}
	printf("listen ok.\n");
	while(player_num < PLAYER_MAX){
		client_sockets[player_num] = accept(
				server_socket,
				(struct sockaddr*)&client_address,
				&client_address_length);
		if (client_sockets[player_num] == ERROR) {
			close(server_socket);
			error("accept");
		}else{
			printf("accept ok.(accepted socket=%d)\n", client_sockets[player_num]);
			write(client_sockets[player_num], &player_num, 1);
			write(client_sockets[player_num], &(player_position[player_num].x), 1);
			write(client_sockets[player_num], &(player_position[player_num].y), 1);
			mode = 1;
			ioctl(client_sockets[player_num], FIONBIO, &mode);
		}
		player_num++;
	}

	printf("4人きたよ\n");
	printMap(map);

	gettimeofday(&time, NULL);
	start_sec = time.tv_sec;
    before_map_send = 0;
	while(1){
		gettimeofday(&time,NULL);
		if(time.tv_sec - start_sec > GAME_TIME){
			printf("ゲーム終了です.\n");
			for(i = 0;i < PLAYER_MAX;i++){
				send(client_sockets[i], "\x1fゲーム終了です.", sizeof("\x1fゲーム終了です."),0);
			}
			break;
		}
        if(before_map_send == 0 || (time.tv_sec*1000+time.tv_usec/1000) - before_map_send > MAP_SEND_INTERVAL){
            for(i = 0;i < PLAYER_MAX;i++){
                for(j = 0;j < MAP_HEIGHT;j++){
                    for(k = 0;k < MAP_WIDTH;k++){
                        send(client_sockets[i], &map[j][k], 1, 0);
                    }
                }
                //send(client_sockets[i], map, MAP_HEIGHT*MAP_WIDTH, 0);
            }
            //printf("map送信\n");
            before_map_send = (time.tv_sec*1000+time.tv_usec/1000);
        }

		for(i = 0;i < PLAYER_MAX;i++){
			temp = recv(client_sockets[i],client_buf[i]+client_recv_len[i], 10, 0); 
			if(temp < 1){
				if(errno != EAGAIN){
					//printf("player%dとの接続が切れました.\n",i);
				}else{
				}
			}else{
				temp += client_recv_len[i];
				for(j = 0;j < (temp / 2);j++){
					move(map, i, &player_position[i], (Direction)client_buf[i][j*2+1]);
				}
				if(temp % 2 != 0){
					client_buf[i][0] = client_buf[i][j*2];
					client_recv_len[i] = 1;
				}else{
					client_recv_len[i] = 0;
				}
				printMap(map);
			}
		}
	}
	close(server_socket);
	for(i = 0;i < PLAYER_MAX;i++){
		close(client_sockets[i]);
	}
}

/**
 * メイン関数
 */
int main(int argc, char* argv[])
{
    int listen_port;

    if (argc != 2) {
        printf("起動時引数が不正です！\n");
        printf("serverの使い方: ./server.out <待ち受けポート番号>\n");
        printf("例）./server.out 12345\n");
        return 0;
    }

    listen_port = atoi(argv[1]);
    start_server(listen_port);

    return 0;
}
