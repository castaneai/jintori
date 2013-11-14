
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


typedef struct _GameObject{
	unsigned char map[MAP_HEIGHT][MAP_WIDTH];
	Position *player_position;
	int player_num;
} GameObject;
GameObject game_obj;
char fill_buffer[MAP_HEIGHT][MAP_WIDTH];


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

void draw_fill(Player num){

	int i, j;
	int inner_color = num + 1;
	for(i = 0;i < MAP_HEIGHT;i++){
		for(j = 0;j < MAP_WIDTH;j++){
			if(fill_buffer[i][j] == 1 && game_obj.map[i][j] < PLAYER_MAX * 2 + 1 && game_obj.map[i][j] != (num + PLAYER_MAX + 1)){
				game_obj.map[i][j] = inner_color;
			}
		}
	}
}

void printFillBuffer(){
	int i,j;
	for(i = 0;i < MAP_HEIGHT;i++){
		for(j = 0;j < MAP_WIDTH;j++){
			printf("%d", fill_buffer[i][j]);
		}
		printf("\n");
	}

}

/**
 * fill_check内で使用される領域チェック用関数
 * */
int fill_check_rec(int x, int y,Player num){
	int line = num + PLAYER_MAX + 1;
	int top = num + PLAYER_MAX*2 + 1;
	fill_buffer[y][x] = 1;

	if(x == 0 || x == MAP_WIDTH-1 || y == 0 || y == MAP_HEIGHT-1){
		return 0;
	}

	if(((game_obj.map[y-1][x]-1)%PLAYER_MAX != num) && fill_buffer[y-1][x] == 0){
		if(!fill_check_rec(x,y-1, num)){
			return 0;
		}
	}

	if(((game_obj.map[y][x+1]-1)%PLAYER_MAX != num) && fill_buffer[y][x+1] == 0){
		if(!fill_check_rec(x+1,y, num)){
			return 0;
		}
	}

	if(((game_obj.map[y+1][x]-1)%PLAYER_MAX != num) && fill_buffer[y+1][x] == 0){
		if(!fill_check_rec(x,y+1, num)){
			return 0;
		}
	}

	if(((game_obj.map[y][x-1]-1)%PLAYER_MAX != num) && fill_buffer[y][x-1] == 0){
		if(!fill_check_rec(x-1,y, num)){
			return 0;
		}
	}

	return 1;
}

/**
 * 領域チェック
 * */
void fill_check(int _x, int _y, int x, int y, Player num){
	int i;
	int line = num + PLAYER_MAX + 1;
	//左側
	if(x > 1 && y > 0 && y < MAP_HEIGHT-1 && _x != x-1 &&(((game_obj.map[y][x-1]-1)%PLAYER_MAX) != num) ){
		i = 1;
		while(x-i >= 0){
			if(game_obj.map[y][x-i] == line){
				memset(fill_buffer, 0, MAP_WIDTH*MAP_HEIGHT);
				fill_buffer[y][x] = 1;
				if(fill_check_rec(x-1, y, num)){

					draw_fill(num);
					break;	
				}
			}
			i++;
		}	
	}

	//右側
	if(x < MAP_WIDTH-2 && y > 0 && y < MAP_HEIGHT-1 && _x != x+1 &&((game_obj.map[y][x+1]-1)%PLAYER_MAX != num) ){
		i = 1;
		while(x+i < MAP_WIDTH){
			if(game_obj.map[y][x+i] == line){
				memset(fill_buffer, 0, MAP_WIDTH*MAP_HEIGHT);
				fill_buffer[y][x] = 1;
				if(fill_check_rec(x+1, y, num)){
					draw_fill(num);	
					break;
				}
			}
			i++;
		}	
	}

	//上側
	if(y > 1 && x > 0 && x < MAP_WIDTH-1 && _y != y-1 &&((game_obj.map[y-1][x]-1)%PLAYER_MAX != num)){
		i = 1;
		while(y-i >= 0){
			if(game_obj.map[y-i][x] == line){
				memset(fill_buffer, 0, MAP_WIDTH*MAP_HEIGHT);
				fill_buffer[y][x] = 1;
				if(fill_check_rec(x, y-1, num)){
					draw_fill(num);	
					break;
				}
			}
			i++;
		}	
	}

	//下側
	if(y < MAP_HEIGHT-2 && x > 0 && x < MAP_WIDTH-1 && _y != y+1 && ((game_obj.map[y+1][x]-1)%PLAYER_MAX != num) ){
		i = 1;
		while(y+i < MAP_HEIGHT){
			if(game_obj.map[y+i][x] == line){
				memset(fill_buffer, 0, MAP_WIDTH*MAP_HEIGHT);
				fill_buffer[y][x] = 1;
				if(fill_check_rec(x, y+1, num)){
					draw_fill(num);	
					break;
				}
			}
			i++;
		}	
	}
}


/*
 * 移動用関数
 * */
void move(int color, Direction direction){
	unsigned char to;
	Position *pos = &(game_obj.player_position[color]);
	int _x, _y;
	_x = pos->x;
	_y = pos->y;
	game_obj.map[pos->y][pos->x] = color + PLAYER_MAX + 1;
	switch (direction){
		case LEFT:
			to = game_obj.map[pos->y][pos->x - 1];
			if(pos->x > 0 && (to < PLAYER_MAX*2+1)) pos->x--;
			break;
		case UP:
			to = game_obj.map[pos->y - 1][pos->x];
			if(pos->y > 0 && (to < PLAYER_MAX*2+1)) pos->y--;
			break;
		case RIGHT:
			to = game_obj.map[pos->y][pos->x + 1];
			if(pos->x < MAP_WIDTH-1 && (to < PLAYER_MAX*2+1)) pos->x++;
			break;
		case DOWN:
			to = game_obj.map[pos->y + 1][pos->x];
			if(pos->y < MAP_HEIGHT - 1 && (to < PLAYER_MAX*2+1)) pos->y++;
			break;
		default:
			break;
	}
	if(((game_obj.map[pos->y][pos->x]-1)%PLAYER_MAX) != color){
		game_obj.map[pos->y][pos->x] = color + PLAYER_MAX*2 + 1;
		fill_check(_x,_y,pos->x,pos->y, color);
	}else{
		game_obj.map[pos->y][pos->x] = color + PLAYER_MAX*2 + 1;
	}
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
 * コマンドを送信する
 */
int send_command(Command command, int socket, char *buf, int length){
	send(socket, &command, 1, 0);
	return send(socket, buf, length, 0);
}


/**
 * コマンドを受信する
 */

int recv_command(int sockets[]){
	static int remain = 0;
	static char buf[PLAYER_MAX][10];
	static int command_remain[PLAYER_MAX] = {0,0,0,0};
	int temp;
	int i, j;
	for(i = 0;i < PLAYER_MAX;i++){
		if(command_remain[i] == 0){
			temp = recv(sockets[i], buf[i], 1, 0);
			if(temp > 0){
				command_remain[i] = 1;
			}
		}
		if(command_remain[i]){
			switch ((Command)buf[i][0]){
				case SEND_DIRECTION:
					temp = recv(sockets[i], buf[i]+1, 1, 0);
					if(temp > 0){
						move(i, (Direction)buf[i][1]);
						command_remain[i] = 0;
						//printMap(game_obj.map);
					}
					break;
				default:
					break;
			}
		}

	}
	return 0;
}

int init_server(port){
	int server_socket; // 受け入れ用のサーバソケットを作る
	int result;
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == ERROR) {
		error("create server_socket");
	}
	bind_socket(server_socket, port);
	printf("bind to port: %d ok.\n", port);

	result = listen(server_socket, DEFAULT_LISTEN_BACKLOG);
	if (result == ERROR) {
		close(server_socket);
		error("listen");
	};
	return server_socket;
}

void init_map(){
	int i,x,y;
	memset(game_obj.map, 0, MAP_HEIGHT * MAP_WIDTH);
	for(i = 0;i < PLAYER_MAX;i++){
		x = game_obj.player_position[i].x;
		y = game_obj.player_position[i].y;
		game_obj.map[y][x] = i + PLAYER_MAX * 2 + 1; 
	}
}

/*
 * プレイヤーを受け付ける
 * */
int accept_player(int server_socket, int *client_sockets){
	int i = 0, mode;
	char buf[3];
	struct sockaddr_in client_address;
	unsigned int client_address_length = sizeof(client_address);
	while(i < PLAYER_MAX){
		client_sockets[i] = accept(
				server_socket,
				(struct sockaddr*)&client_address,
				&client_address_length);
		if (client_sockets[i] == ERROR) {
			close(server_socket);
			error("accept");
		}else{
			printf("accept ok.(accepted socket=%d)\n", client_sockets[i]);
			buf[0] = i;
			buf[1] = game_obj.player_position[i].x;
			buf[2] = game_obj.player_position[i].y;
			send_command(REGISTER_PLAYER, client_sockets[i], buf, 3);
			mode = 1;
			ioctl(client_sockets[i], FIONBIO, &mode);
		}
		i++;
	}

	return 0;
}

int getmillisecond(){
	struct timeval time;
	gettimeofday(&time, NULL);
	return (time.tv_sec*1000+time.tv_usec/1000);
}



void finish_game(int *client_sockets){
	int i,j;
	short count[PLAYER_MAX] = {0,0,0,0};

	for(i = 0;i < MAP_HEIGHT;i++){
		for(j = 0;j < MAP_WIDTH;j++){
			if(game_obj.map[i][j] != 0){
				count[(game_obj.map[i][j]-1) % PLAYER_MAX]++;
			}
		}
	}


	for(i = 0;i < PLAYER_MAX;i++){
		send_command(FINISH_GAME, client_sockets[i], (char *)count, 8);
	}


}

/**
 * サーバーを開始する
 * @param port 待ち受けポート番号
 */
void start_server(unsigned short port)
{
	int server_socket;
	int client_sockets[PLAYER_MAX];
	char player_num = 0;
	int i, j;
	Position player_position[] = {{MAP_WIDTH/2-1,MAP_HEIGHT/2-1},{MAP_WIDTH/2+1,MAP_HEIGHT/2-1},{MAP_WIDTH/2-1,MAP_HEIGHT/2+1},{MAP_WIDTH/2+1,MAP_HEIGHT/2+1}};
	int start_time;
	int last_send_time;
	int now_time;
	int left_time;
	game_obj.player_position = (Position *)&player_position;
	game_obj.player_num = PLAYER_MAX; 

	//マップを初期化する
	init_map();

	// 受け入れ用のサーバソケットを作る
	server_socket = init_server(port);    

	printf("listen ok.\n");	

	accept_player(server_socket, client_sockets);

	printf("プレイヤーが揃いました.ゲームを開始します.\n");


	start_time = getmillisecond();
	last_send_time = start_time;
	left_time = GAME_TIME;

	for(i = 0;i < PLAYER_MAX;i++){
		send_command(START_GAME, client_sockets[i], "", 0);
		send_command(SEND_MAP, client_sockets[i], (char *)game_obj.map, MAP_HEIGHT*MAP_WIDTH);
		send_command(TIME_UPDATE, client_sockets[i], (char *)&left_time, sizeof(int));

	}

	while(1){	
		now_time = getmillisecond();
		if(now_time - start_time > GAME_TIME){
			printf("ゲーム終了です.\n");
			finish_game(client_sockets);
			break;
		}
		
		if(now_time - last_send_time > MAP_SEND_INTERVAL){
			left_time = GAME_TIME - (now_time-start_time);
			for(i = 0;i < PLAYER_MAX;i++){
				send_command(SEND_MAP, client_sockets[i], (char *)game_obj.map, MAP_HEIGHT*MAP_WIDTH);
				//send_command(TIME_UPDATE, client_sockets[i], (char *)&left_time, sizeof(int));
			}
			last_send_time = now_time;
		}

		recv_command(client_sockets);
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
