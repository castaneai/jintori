#include <ncurses.h>

#define MAX_MAP_WIDTH 50
#define MAX_MAP_HEIGHT 50

void draw(unsigned char map[], int width, int height){
	
	int i,j;
    int x, y;
    char input;
	initscr(); cbreak(); noecho();
	start_color();
	
	init_pair(1, COLOR_WHITE, COLOR_RED);
	init_pair(2, COLOR_WHITE, COLOR_GREEN);
	init_pair(3, COLOR_WHITE, COLOR_YELLOW);
	init_pair(4, COLOR_WHITE, COLOR_BLUE);
	init_pair(5, COLOR_WHITE, COLOR_MAGENTA);

    x = 1;
    y = 0;

    while (1) {

        // draw map
        for(i = 0;i < height;i++){
            for(j = 0;j < width;j++){
                if(map[i*width + j] == 255)/*黒*/{
                    mvprintw(i, j, "%c", ' ');
                }
                else if(map[i*width + j] > 4) /* @ */{
                    attron(COLOR_PAIR(map[i*width + j]+1-5));
                    mvprintw(i, j, "%c", '@');
                }
                else{	/* Q */
                    attron(COLOR_PAIR(map[i*width + j]+1));
                    mvprintw(i, j, "%c", 'Q');
                }
            }
        }

        input = (char)getch();

        switch (input) {
            case 'h':
                x--;
                break;

            case 'j':
                y++;
                break;

            case 'k':
                y--;
                break;

            case 'l':
                x++;
                break;

            default:
                continue;
        }
    }

	endwin();
}	

int main(void)
{
    unsigned char map[2*2] = {255, 5, 1, 1};
    draw(map, 2, 2);
    return 0;
}
