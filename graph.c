
#include <stdlib.h>
#include <stdio.h>

int main (int argc, char *argv[])
{
	int index;
	float x, y;

	if (js_open() == 0)	{
		fprintf(stderr, "Connection failed.\n");
		exit(EXIT_FAILURE);
	}

	js_initialize();

	while (!js_eof()) {
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