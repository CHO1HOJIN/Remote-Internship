int x[10];
int y[10];
int z[10];
int i;

int add()
{
	x[i] = i;
	y[i] = i * i;
	z[i] = x[i] + y[i];
	i++;
}

void f()
{
	for(i = 0; i < 10; i++)
	{
		add();
	}
}

int main()
{
	f();
}
