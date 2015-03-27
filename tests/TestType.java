class TestType
{
	TestType o = null;
	char c = 'c';
	byte b = 1;
	short s = 1;
	long l = 1;
	float f = 0.1f;
	double d = 0.1d;

	public TestType(int a1, char a2)
	{
		c = a2;
		b = 0;
		s = (short) a1;
		l = a1;
		f = a1;
		d = a1;
	}

	public static void doTest(TestType o1, TestType o2)
	{
		o1.o = o2;
	}

	public static void main(String args[])
	{
		TestType a = new TestType(6, 'a');
		TestType b = new TestType(6, 'b');
		doTest(a, b);
		System.out.println("hello");
	}
}
