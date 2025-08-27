int a[10] = {1, 1};
int i;
int main()
{
	for(i = 2; i < 10; i++)
	{
		a[i] = a[i-1] + a[i-2];
	}	
}
