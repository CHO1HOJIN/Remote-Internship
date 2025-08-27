#include <stdio.h>
void C(){}
void B() { C(); }
void A() { }

int main() {

	void (*p)();
	A();
	p = &A;
	p = &B;
	(*p)();

}

/*
C:
B: C
A: 
main: A B
*/