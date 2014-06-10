class Test
{
	int test = 0;

	public Test(int a)
	{
		test = a;
	}

	public int doTest(int b)
	{
		test = test + b;
		return test;
	}

	public static void main(String args[])
	{
		Test a = new Test(6);
		int b;

		b = a.doTest(4);
		System.out.println("hello");
	}
}
