class PTest
{
	int t;

	public PTest()
	{
		t = 0;
	}

	public void doTest(int b)
	{
		System.out.println("PTEST");
	}
}

class Test extends PTest
{
	int test = 0;
	static int testa = 0;

	public Test(int a)
	{
		test = a;
	}

	public void doTest(int b)
	{
		test = test + b;
		System.out.println("TEST");
	}

	public int getTest()
	{
		return test;
	}

	public static void main(String args[])
	{
		PTest a = new Test(6);
		int b;

		a.doTest(4);
		Test.testa = 1;
//		b = a.getTest();
//		System.out.println("hello " + b);

		Test c[] = new Test[4];
		for (int i = 0; i < 4; i++)
			c[i] = new Test(i);

		for (int i = 0; i < 4; i++)
			System.out.println("[" + i + "]:" + c[i].getTest());
	}
}
