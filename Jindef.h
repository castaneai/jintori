#ifndef _JINDEF_H_
#define _JINDEF_H_

#define PLAYER_MAX 4
#define MAP_WIDTH 50
#define MAP_HEIGHT 20
#define GAME_TIME 3000
#define MAP_SEND_INTERVAL 120 

typedef enum _Color{
    EMPTY,
    
    RED_INNER,
    GREEN_INNER,
    YELLOW_INNER,
    BLUE_INNER,
    
    RED_TRACK,
    GREEN_TRACK,
    YELLOW_TRACK,
    BLUE_TRACK,
    
    RED_PLAYER,
    GREEN_PLAYER,
    YELLOW_PLAYER,
    BLUE_PLAYER
} Color;

typedef enum _Direction{
    LEFT,
    DOWN,
    UP,
    RIGHT,
    
    UNKNOWN
} Direction;


typedef struct _Position{
	char x,y;
} Position;

#endif
