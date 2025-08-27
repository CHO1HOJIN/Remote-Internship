#include <stdio.h>

struct s{
	void (*q)();
	int value;
} s1;
void E() {};
void D() {}
void C() {}
void B() { C(); }
void A() { B(); }

int main() {
	void (*p)();
	void (*q)();
	int x = 12;
	p = E;
	q = B;
	(*p)();
	if(x == 1)
	{
		p = C;
	}
	else
	{
		q = A;
		(*q)();
	}
	q = D;
	(*q)();

}

/*
E:
D:
C:
B: C
A: B
main: E A D
*/