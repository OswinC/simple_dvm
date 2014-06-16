class PTest
{
	int t;

	public PTest()
	{
		t = 0;
	}
}

class Test extends PTest
{
	int test = 0;

	public Test(int a)
	{
		test = a;
	}

	public void doTest(int b)
	{
		test = test + b;
	}

	public int getTest()
	{
		return test;
	}

	public static void main(String args[])
	{
		Test a = new Test(6);
		int b;

		a.doTest(4);
		b = a.getTest();
		System.out.println("hello " + b);

		Test c[] = new Test[4];
		for (int i = 0; i < 4; i++)
			c[i] = new Test(i);
	}
}
