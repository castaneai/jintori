// 汎用ライブラリ
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

// ネットワークライブラリ
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

// ncurses
#include <ncurses.h>

// スレッド
#include <pthread.h>

// 陣取りゲームのクライアント／サーバー共通定義
#include "Jindef.h"

#define SOCKET_ERROR -1

// ゲームプレイ中かどうか
// プレイ中なら1, ゲームが終了したら0になる
// メインスレッドと描画スレッドの連絡を取るために
// グローバル変数にしてある
int is_playing;

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
 * ゲーム終了後の成績データ
 */
typedef struct
{
    Color color;
    int tile_count;
} ScoreData;

/**
 * サーバーにコマンドを送る
 */
void send_command(int sock, Command command)
{
    char message[] = {command};
    write(sock, message, sizeof(message));
}

/**
 * サーバーから次のコマンドを受け取る
 */
Command recv_command(int sock)
{
    char command;
    read(sock, &command, 1);
    return command;
}

/**
 * サーバーに自分が進んだ方向を送る
 * @param sock サーバーのソケット
 * @param dir 進んだ方向
 */
void send_move(int sock, Direction dir)
{
    send_command(sock, SEND_DIRECTION);
    char message[] = {dir};
    write(sock, message, sizeof(message));
}

/**
 * サーバーから初期データを受け取る
 * @param sock サーバーのソケット
 * @param output_data 初期データを受け取る構造体ポインタ
 */
void recv_start_data(int sock, StartData* output_data)
{
    if (recv_command(sock) != REGISTER_PLAYER) {
        perror("register player command expected. but other command was received");
        exit(1);
    }
    read(sock, &(output_data->color), 1);
    // サーバーからくる色の番号は0始まりだが,
    // ncursesの色ペアは1始まりなので，＋1する
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
    curs_set(0); // カーソル非表示
    keypad(stdscr, TRUE); // 十字キーを有効化
    nodelay(stdscr, TRUE); // getchなどの入力関数を非同期にする

    start_color(); // 色を有効化
    init_pair(1, COLOR_WHITE, COLOR_RED);
    init_pair(2, COLOR_WHITE, COLOR_GREEN);
    init_pair(3, COLOR_WHITE, COLOR_YELLOW);
    init_pair(4, COLOR_WHITE, COLOR_BLUE);
    init_pair(5, COLOR_WHITE, COLOR_CYAN); // 背景色
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
    // 左から 何もない場所，軌跡，プレイヤー
    char draw_chars[] = {' ', '.', '@'};
    char draw_char;
    int index = tile > 0 ? (int)((tile - 1) / 4) : 0;
    int color_id = tile > 0 ? (tile - 1) % 4 + 1 : 5;

    draw_color_pair = COLOR_PAIR(color_id);
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
int recv_complete(int sock, char* buf, int length)
{
    int total_read_count = 0;
    int recv_result;

    while (total_read_count < length) {
        recv_result = read(sock, buf + total_read_count, length - total_read_count);
        if (recv_result < 1) {
            return FALSE;
        }
        total_read_count += recv_result;
        refresh();
    }
    return TRUE;
}

/**
 * ゲーム結果をサーバーから受け取る
 * @param sock サーバーのソケット
 * @param sorted_scores 結果を格納する成績データの配列 成績の高い順にソートされている
 */
void recv_result_scores(int sock, ScoreData sorted_scores[])
{
    int i, k;
    int temp;
    short scores[PLAYER_MAX];
    read(sock, scores, sizeof(scores));
    int sorted_colors[] = {0, 1, 2, 3};

    for (i = 0; i < PLAYER_MAX - 1; i++) {
        for (k = 0; k <PLAYER_MAX - 1; k++) {
            if (scores[k] < scores[k+1]) {
                temp = sorted_colors[k];
                sorted_colors[k] = sorted_colors[k+1];
                sorted_colors[k+1] = temp;

                temp = scores[k];
                scores[k] = scores[k+1];
                scores[k+1] = temp;
            }
        }
    }

    for (i = 0; i < PLAYER_MAX; i++) {
        sorted_scores[i].color = sorted_colors[i] + 1;
        sorted_scores[i].tile_count = scores[i];
    }
}
/**
 * 別スレッドで実行される，マップ描画関数
 * @param sock スレッドの引数は必ずポインタなので，サーバーのソケットへのポインタである
 */
void draw_map_thread(int* sock)
{
    const int MAP_SIZE = MAP_WIDTH * MAP_HEIGHT;
    char map_data[MAP_SIZE];
    int recv_result;
    char command;
	int left_time;

	while (1) {
        command = recv_command(*sock);
        if (command == FINISH_GAME) break;
		if (command == TIME_UPDATE){
			recv_result = recv_complete(*sock, (char *)&left_time, sizeof(int));
			if(recv_result == FALSE){
				perror("recv_complete");
				break;
			}
   			mvprintw(6, MAP_WIDTH + 2, "%d seconds left", left_time/1000);
		}
        if (command == SEND_MAP) {
            // マップデータは長いのでTCPの仕様により複数のreadに分割されることがある
            // よって確実に受取る関数を使っている
            recv_result = recv_complete(*sock, map_data, MAP_SIZE);
            if (recv_result == FALSE) {
                perror("recv_complete");
                break;
            }
            draw_map(map_data);
        }
    }
    // ゲーム終了をメインスレッドに知らせるために
    // グローバル変数を書き換える
    is_playing = 0;
}

/**
 * ncursesの画面に自分の色が何色か表示する
 * @param color 自分の色
 */
void show_my_color(Color color)
{
    attron(COLOR_PAIR(color));
    mvprintw(5, MAP_WIDTH + 1, "%c", ' '); 
    attroff(COLOR_PAIR(color));
    mvprintw(5, MAP_WIDTH + 2, "<= your color");
}

void show_result(ScoreData score_data[], Color my_color)
{
    int i;

    erase();
    refresh();
    mvprintw(9, 10, "FINISH GAME!!");
    for (i = 0; i < PLAYER_MAX; i++) {
        mvprintw(11+i, 8, "%d ", i+1);
        attron(COLOR_PAIR(score_data[i].color));
        mvprintw(11+i, 10, "%c", ' ');
        attroff(COLOR_PAIR(score_data[i].color));
        mvprintw(11+i, 11, ": %d tiles", score_data[i].tile_count);
        if (score_data[i].color == my_color) {
            mvprintw(11+i, 22, "<= You!");
        }
    }
    mvprintw(11+PLAYER_MAX+1, 10, "Press enter to exit game. thank you for playing!");
}

/**
 * ゲームを始める
 */
void start_game(int sock, const StartData* start_data)
{
    pthread_t draw_thread_id;
    Direction dir;
    ScoreData score_data[PLAYER_MAX];
    char trash_buf[10];

    // ncurses初期化
    init_ncurses();

    // 自分の色を表示
    show_my_color(start_data->color);
    
    // 描画スレッドを開始
    is_playing = 1;
    pthread_create(&draw_thread_id, NULL, (void*)draw_map_thread, &sock);

    // ゲームプレイ中はずっと入力を待ち続ける（いわゆるゲームループ）
    while (is_playing) {
        dir = input_dir();
        if (dir != UNKNOWN) {
            send_move(sock, dir);
        }
    } 
    // ゲームが終了したので入力を同期処理に戻す
    nodelay(stdscr, FALSE);

	//getstrが下キーで終了しないようにする
	keypad(stdscr, FALSE);

    // 結果をサーバーから受け取り，表示する
    recv_result_scores(sock, score_data);

    // ゲーム結果を表示する
    show_result(score_data, start_data->color);

    // Enterキーが押されるまで待機
    getstr(trash_buf);

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
 * ゲーム開始の合図を待つ
 */
void wait_game_start(int sock)
{
    char command;
    read(sock, &command, 1);
    // ゲーム開始の合図以外が最初に来てしまったらエラーを出して終了する
    if (command != START_GAME) {
        perror("wait start game");
        exit(1);
    }
}

/**
 * メイン関数
 */
int main(int argc, char* argv[])
{
    int sock;
    int forked_pid;
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

    // ゲーム開始の合図を待つ
    wait_game_start(sock);
    
    // ゲームスタート
    start_game(sock, &start_data);

    close(sock);
    return 0;
}
