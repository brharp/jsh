#include <stdlib.h>
#include <stdio.h>
#include <search.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define ALIVE '@'
#define DEAD  '.'
#define EOL   '$'
#define STEP  '!'
#define QUIT  'q'

#define CELL_SIZE 10

int a(int, int, int);
int m(int,int,int);


/* Neighbours of (x,y) in generation g. */

int n( int x, int y, int g )
{
        return m( x - 1, y - 1, g )
             + m( x,     y - 1, g )
             + m( x + 1, y - 1, g )
             + m( x - 1, y,     g )
             + m( x + 1, y,     g )
             + m( x - 1, y + 1, g )
             + m( x,     y + 1, g )
             + m( x + 1, y + 1, g );
}


/* Dictionary key for (x,y) in generation g. */

char *i(int x, int y, int g)
{
        static char s[256];

        sprintf(s, "%d,%d,%d", x, y, g);

        return strdup(s);
}


/* An empty cell becomes alive if it has 3 neighbours,
   and a living cell survives if it has 2 neighbours.   */

int a(int x, int y, int g)
{
        return g == 0 ? 0 :
                n(x, y, g-1) == 3 ||
                n(x, y, g-1) == 2 && m(x, y, g-1);
}


/* Memoizes the results of a(). */

int m(int x, int y, int g)
{
        ENTRY e, *ep;

        e.key = i(x, y, g);
        ep = hsearch(e, FIND);
        if (!ep) {
                e.data = (void *)(intptr_t)a(x, y, g);
                ep = hsearch(e, ENTER);
                if (!ep) {
                        perror("m");
                        exit(EXIT_FAILURE);
                }
        }
        else {
                free(e.key);
        }

        return (int)(intptr_t)(ep->data);
}


/* clear rectangle */

void clr(int x, int y, int w, int h)
{
	printf("displaylist.push((cx) => cx.clearRect(%d, %d, %d, %d))\n", x, y, w, h);
	fflush(stdout);
}


/* display text */

void dt(const char *s, int x, int y)
{
	clr(x - 2, y - 16, 100, 18);
	printf("displaylist.push((cx) => cx.fillText('%s', %d, %d))\n", s, x, y);
	fflush(stdout);
}


/* fill rectangle */

void fr(int x, int y, int w, int h)
{
	printf("displaylist.push((cx) => cx.fillRect(%d, %d, %d, %d))\n", x, y, w, h);
	fflush(stdout);
}


/* stroke rectangle */

void sr(int x, int y, int w, int h)
{
	printf("displaylist.push((cx) => cx.strokeRect(%d, %d, %d, %d))\n", x, y, w, h);
	fflush(stdout);
}


/* paint */

void p()
{
	printf("paint()\n");
	fflush(stdout);
}


/* Display generation G, starting from X, Y on a WxH screen.
d(g, x, y, w, h) =
        for i = 0 to w
                for j = 0 to h
                        pixel(i, j) = a(i+x, j+y)
*/
void d (char *s, int g, int x, int y, int w, int h)
{
        int i, j;
        char t[80];
        const int z = CELL_SIZE;

        clr(0, 0, z * w, z * h);

        for (j = 0; j < h; j++) {
                for (i = 0; i < w; i++) {
                        if (m(i+x, j+y, g))
                        	fr(i * z, j * z, z, z);
                        else
                        	sr(i * z, j * z, z, z);
                }
        }

        sprintf(t, "Generation: %d", g);
        dt(t, 10, 10);

        p();
}


#define WIDTH  20
#define HEIGHT 20
#define NEL 1000000

int main (int argc, char *argv)
{
        int x = 0, y = 0, g = 0, c;
        char v[HEIGHT*(WIDTH+1)+1];
        ENTRY e;

        hcreate(NEL);

        while ((c=getchar()) != EOF) {
                switch (c) {
                        case EOL:
                                x=0; y++;
                                break;
                        case ALIVE:
                                e.key = i(x, y, g);
                                e.data = (void *) 1;
                                hsearch(e, ENTER);
                                // fall through
                        case DEAD:
                                x++;
                                break;
                        case STEP:
				d(v, g++, 0, 0, WIDTH, HEIGHT);
                                break;
                        case QUIT:
                        	goto quit;
                }
        }
quit:

        hdestroy();
}
