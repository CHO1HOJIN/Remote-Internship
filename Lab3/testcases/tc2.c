#include <stdio.h>

struct s{
	void (*q)();
	int value;
} s1;

void F() { printf("f must be invoked!\n"); }
void E() { 
	s1.q = &F;
	s1.q();
}
void D() {  }
void C() { D(); E(); }
void B() { C(); }
void A() { B(); }

int main() {

	A();

}


/*
F: printf
E: q
D: 
C: D E
B: C
A: B
main: A
*/