class TestBr2 {
	public int b;
	public TestBr2()
	{
		b = -1;
	}
}

class TestBr
{
	public static void main(String args[]) {
		TestBr2 o = new TestBr2();
		if (o.b > 0)
			System.out.println("Test 1");
		else
			System.out.println("Test 2");
	}
}
