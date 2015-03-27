class TestClinit
{
	public static int a = 12;

	public TestClinit()
	{
		a += 5;
	}
	
	public static void main(String args[])
	{
		TestClinit o = new TestClinit();

		System.out.println("" + TestClinit.a);
	} 
}
