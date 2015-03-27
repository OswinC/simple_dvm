class TestChild extends TestParent {
	public int a;
	public String b;
	public char[] c;

	public TestChild(int n) {
		a = n;
		super.a = n;
		super.b = 0.3f;
		((TestGrand) this).x = n;
	}

	public static void main(String args[]) {
		TestChild o = new TestChild(15); 
		System.out.println("hello " + o.a);
	} 
}

