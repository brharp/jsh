
#include <stdlib.h>
#include <stdio.h>
#include "js.h"

#define MENUHIT_TAG 1

#define WIDTH   500
#define HEIGHT  500

void defjs(const char *s)
{
	js_write(s, strlen(s));
}

void js_begincurve()
{
	defjs("dl = []");
}

void js_endcurve()
{
	char s[BUFSIZ];
	sprintf(s, "paint(%s, %d, %d)", "ctx", WIDTH, HEIGHT);
	defjs(s);
}

int js_menuhit(int *index)
{
	int tag, n;
	char fmt[80], buf[BUFSIZ];
	js_read(buf, BUFSIZ);
	sprintf(fmt, "%d %%d", MENUHIT_TAG);
	return sscanf(buf, fmt, index) == 1;
}

void js_lineto(float x, float y)
{
	char s[BUFSIZ];
	sprintf(s, "dl.push((ctx) => ctx.lineTo(%f, %f))", x, y);
	defjs(s);
}

void js_initialize()
{
	char s[BUFSIZ];
	defjs(
		"paint = function (ctx, width, height) {"
		"  ctx.fillStyle = 'white'"
		"  ctx.fillRect(0, 0, width, height)"
		"  ctx.save()"
		"  ctx.scale(width/13, height/3)"
		"  ctx.clearRect(0, 0, 13, 3)"
		"  ctx.translate(0, 1.5)"
		"  ctx.beginPath()"
		"  ctx.moveTo(0, 0)"
		"  ctx.lineTo(13, 0)"
		"  ctx.moveTo(0, 0)"
		"  dl.forEach((f) => f(ctx))"
		"  ctx.restore()"
		"  ctx.stroke()"
		"}"
	);
	defjs("cvs = document.createElement('canvas')");
	sprintf(s, "cvs.width = %d", WIDTH); defjs(s);
	sprintf(s, "cvs.height = %d", HEIGHT); defjs(s);
	defjs("ctx = cvs.getContext('2d')");
	defjs("document.body.appendChild(cvs)");
	js_begincurve();
}

int main (int argc, char *argv[])
{
	int index;
	float x, y;

	if (js_open() < 0)	{
		fprintf(stderr, "Connection failed.\n");
		exit(EXIT_FAILURE);
	}

	js_initialize();

	while (1) {
		if (js_menuhit(&index)) {
			js_begincurve();
			for (x = 0; x <= 13; x += .1) {
				switch (index) {
					case 0: y = sin(x); break;
					case 1: y = cos(x); break;
					case 2: y = sin(x) * exp(-x/3) * 3; break;
					case 3: y = sin(x) + .1*sin(x*5+1); break;
					default: y = 0;
				}
				js_lineto(x, y);
			}
			js_endcurve();
		} else break;
	}

	js_close();
}