#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

Image *bg, *fg, *innergridbg, *gridbg, *snakeheadbg, *snakebg, *applebg;

typedef enum direction Direction;
enum
{
	DIRECTION_UP,
	DIRECTION_DOWN,
	DIRECTION_LEFT,
	DIRECTION_RIGHT
} direction;

typedef struct apple Apple;
struct apple
{
	int x, y, count;
};
Apple apple;

typedef struct snake Snake;
struct snake
{
	int x, y;
	Direction dir;
	Snake *next;
};

Snake *head, *tail;

int running;

#define WIDTH 800
#define HEIGHT 600
#define INTERVAL 110
#define GRID_SIZE 24
#define GRID_DIM 550
#define BORDER 4

#define cell_size (int)(GRID_DIM / GRID_SIZE)
#define grid_x (int)(WIDTH/2) - (GRID_DIM / 2)
#define grid_y (int)((HEIGHT/2) - (GRID_DIM / 2) + 22)

void
apple_make(void)
{
	Snake *p;
	int ok;

	do{
		ok = 1;
		apple.x = rand() % GRID_SIZE;
		apple.y = rand() % GRID_SIZE;
		p = head;
		while(p){
			if(apple.x == p->x && apple.y == p->y)
				ok=0;
			p = p->next;
		}
	}while(!ok);

	apple.count++;
}

void
draw_cell(int x, int y, Image* col)
{
	draw(screen, Rect(screen->r.min.x+grid_x+(x*cell_size),
		screen->r.min.y+grid_y+(y*cell_size),
		screen->r.min.x+grid_x+(x*cell_size)+cell_size,
		screen->r.min.y+grid_y+(y*cell_size)+cell_size),
		col, nil, ZP);
}

void
apple_render(void)
{
	Apple *p = &apple;
	// TODO: Render more than one apple

	draw_cell(p->x, p->y, applebg);
}

void
snake_init(void)
{
	Snake *s;

	s = malloc(sizeof(*s));
	if(!s) sysfatal("snake_init: %r");

	s->x = rand() % GRID_SIZE;
	s->y = rand() % GRID_SIZE;
	s->dir = rand() % 3;
	s->next = nil;

	head = s;
	tail = s;
}

void
snake_render(void)
{
	Snake *p = head;

	if(!p) sysfatal("snake_draw: %r");
	draw_cell(p->x, p->y, snakeheadbg);

	p = p->next;
	while(p){
		draw_cell(p->x, p->y, snakebg);
		p = p->next;
	}
}

void
snake_move(void)
{
	Snake *p = head;
	int prev_x, prev_y, prev_dir;

	if(!p) sysfatal("snake_move: %r");
	prev_x = p->x;
	prev_y = p->y;
	prev_dir = p->dir;
	
	switch(p->dir){
	case DIRECTION_UP:
		p->y--;
		break;
	case DIRECTION_DOWN:
		p->y++;
		break;
	case DIRECTION_LEFT:
		p->x--;
		break;
	case DIRECTION_RIGHT:
		p->x++;
		break;
	}

	while((p = p->next)){
        int new_x = p->x;
        int new_y = p->y;
        int new_dir = p->dir;

        p->x = prev_x;
        p->y = prev_y;
        p->dir = prev_dir;

        prev_x = new_x;
        prev_y = new_y;
        prev_dir = new_dir;
	}
}

void
snake_extend(void)
{
	Snake *n;

	n = malloc(sizeof(Snake));
	if(!n) sysfatal("snake_extend: %r");
	
	switch(tail->dir){
	case DIRECTION_UP:
		n->x = tail->x;
		n->y = tail->y+1;
		break;
	case DIRECTION_DOWN:
		n->x = tail->x;
		n->y = tail->y-1;
		break;
	case DIRECTION_LEFT:
		n->x = tail->x-1;
		n->y = tail->y;
		break;
	case DIRECTION_RIGHT:
		n->x = tail->x+1;
		n->y = tail->y;
		break;
	}

	n->next = nil;
	tail->next = n;
	tail = n;
}

void
game_logic(void)
{
	Snake* p = head;

	if(!p) sysfatal("snake_move: %r");
	if(p->x < 0 || p->x >= GRID_SIZE || p->y < 0 || p->y >= GRID_SIZE){
		print("You touched the wall, loser. %d point(s) to you.\n", apple.count);
		exits(nil);
	}

	while((p = p->next)){
		if(head->x == p->x && head->y == p->y){
			print("Cannibal! You lose. %d point(s) to you.\n", apple.count);
			exits(nil);
		}
	}

	if(head->x == apple.x && head->y == apple.y){
		apple_make();
		snake_extend();
	}
}


void
redraw(void)
{
	Rectangle r;
	char buf[64];

	draw(screen, screen->r, bg, nil, ZP);

	sprint(buf, "score: %d", apple.count);
	string(screen,
		addpt(Pt(screen->r.min.x, screen->r.min.y),
		Pt((Dx(screen->r)/2)-(stringwidth(display->defaultfont, buf)/2), display->defaultfont->height/2)),
		fg, ZP, display->defaultfont, buf);

	r.min = addpt(screen->r.min, Pt(grid_x-(BORDER/2), grid_y-(BORDER/2)));
	r.max = addpt(screen->r.min, Pt(grid_x+(GRID_SIZE*cell_size)+BORDER, grid_y+(GRID_SIZE*cell_size)+BORDER));
	draw(screen, r, innergridbg, nil, ZP);

	r.min = subpt(r.min, Pt(BORDER/2, BORDER/2));
	border(screen, r, BORDER, gridbg, ZP);

	apple_render();
	snake_render();
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("resize failed: %r");
	redraw();
}

void
resize(int x, int y)
{
	int fd;

	if((fd = open("/dev/wctl", OWRITE))){
		fprint(fd, "resize -dx %d -dy %d", x, y);
		close(fd);
	}
}

void
main(int, char*[])
{
	Event ev;
	int e, timer;
	char *options1[] = {"", "pause", "quit", nil};
	Menu middlemenu = {options1};

	snake_init();
	apple_make();
	apple.count = 0;

	if(initdraw(nil, nil, argv0) < 0)
		sysfatal("initdraw: %r");

	// TODO: read from /dev/theme
	bg = allocimagemix(display, DPalebluegreen, DWhite);
	fg = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1, DBlack);
	gridbg = allocimage(display, Rect(0,0,1,1), RGB24, 1, 0x4A4A4AFF);
	innergridbg = allocimagemix(display, DPalebluegreen, 0x66666655);
	snakeheadbg = allocimage(display, Rect(0,0,1,1), RGB24, 1, 0x7DFDA4FF);
	snakebg = allocimage(display, Rect(0,0,1,1), RGB24, 1, 0x9EFEBBFF);
	applebg = allocimage(display, Rect(0,0,1,1), RGB24, 1, 0xE47674FF);

	eresized(0);
	einit(Emouse | Ekeyboard);
	timer = etimer(0, INTERVAL);
	running = 1;
	resize(WIDTH, HEIGHT);

	for(;;){
		e = event(&ev);
		if(e == Ekeyboard){
			switch(ev.kbdc){
			case 'q':
			case Kdel:
				exits(nil);
				break;
			case Kup:
			case 'k':
				if(head->dir == DIRECTION_DOWN) break;
				head->dir = DIRECTION_UP;
				break;
			case Kdown:
			case 'j':
				if(head->dir == DIRECTION_UP) break;
				head->dir = DIRECTION_DOWN;
				break;
			case Kleft:
			case 'h':
				if(head->dir == DIRECTION_RIGHT) break;
				head->dir = DIRECTION_LEFT;
				break;
			case Kright:
			case 'l':
				if(head->dir == DIRECTION_LEFT) break;
				head->dir = DIRECTION_RIGHT;
				break;
			}
		} else if(e == Emouse){
			if(ev.mouse.buttons & 3){
				if(emenuhit(2, &ev.mouse, &middlemenu)==2) exits(nil);
				if(emenuhit(2, &ev.mouse, &middlemenu)==1){
					running=!running;
					if(!running){
						snprint(options1[1], 6, "play");
					} else {
						snprint(options1[1], 6, "pause");
					}
				}
			}
		} else if(e == timer){
			if(running){
				snake_move();
				game_logic();
				redraw();
			}
		}
	}
}
