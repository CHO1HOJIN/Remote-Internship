#include <stdio.h>
void C(){}
void B() { C(); }
void A() { }

int main() {

	void (*p)();
	A();
	int x = 12;
	if(x > 20) { 
		p = &C; 
	} else { 
		p = &B; 
	}
	if(x)
		x = 22;
	else
		x = 1;
	(*p)();

}

/*
C:
B: C
A:
main: A C B
*/