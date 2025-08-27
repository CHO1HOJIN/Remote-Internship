#include <unistd.h>
int x, y, z;
int *a;
int main()
{
	x = 1;
	y = x  + 1;
	z = x + y;
	a = &z;
	*a = 1;
	return z;
}
