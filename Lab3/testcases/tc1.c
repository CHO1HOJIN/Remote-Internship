void F() {  }
void E() {  }
void D() {  }
void C() { D(); E(); }
void B() { C(); }
void A() { B(); }

int main() {

	void (*p)();
	A();
	p = &C;
	(*p)();
}

/*
F:
E:
D:
C: D E
B: C
A: B
main: A C
*/
