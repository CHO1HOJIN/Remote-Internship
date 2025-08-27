#include <stdio.h>

struct s{
	int value;
	void (*q)();
} s1;
void E() {};
void D() {}
void C() {}
void B() { C(); }
void A() { B(); }

int main() {
	struct s *s2 = &s1;
	s1.value = 1;
	void (*p)();
	void (*q)();
	A();
	p = &A;
	p = &B;
	(*p)();

	q = C;
	q = p;
	(*q)();
	
	s2->q = q;
	s2->q = E;
	s2->q();
}

/*
E:
D:
C:
B: C
A: B
main: A B E
*/