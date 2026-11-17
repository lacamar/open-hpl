// Array and string add-on behaviour as scripts see it (Amnesia-style engine: string is the ref type).
// Run with OPENHPL_AS_SUITE=<this file> Soma.bin.aarch64; the output is compared with suite-amnesia.out.

void say(int v) { say("" + v); }
void say(uint v) { say("" + v); }

int live = 0;
int dtors = 0;

class Item
{
	int k;
	int id;
	Item() { k = 0; id = 0; live++; }
	Item(int k_, int id_) { k = k_; id = id_; live++; }
	Item(const Item &in o) { k = o.k; id = o.id; live++; }
	~Item() { live--; dtors++; }
	Item &opAssign(const Item &in o) { k = o.k; id = o.id; return this; }
	int opCmp(const Item &in o) const { if (k < o.k) return -1; if (k > o.k) return 1; return 0; }
	bool opEquals(const Item &in o) const { return k == o.k; }
}

class Plain
{
	int v;
	Plain() { v = 0; }
	Plain(int v_) { v = v_; }
}

string ints(const array<int> &in a)
{
	string s = a.length() + ":[";
	for (uint i = 0; i < a.length(); i++)
	{
		if (i > 0) s += ",";
		s += a[i];
	}
	return s + "]";
}

string flts(const array<float> &in a)
{
	string s = a.length() + ":[";
	for (uint i = 0; i < a.length(); i++)
	{
		if (i > 0) s += ",";
		s += a[i];
	}
	return s + "]";
}

string ids(const array<Item@> &in a)
{
	string s = a.length() + ":";
	for (uint i = 0; i < a.length(); i++)
	{
		if (a[i] is null) { s += " null"; continue; }
		s += " " + a[i].k + "/" + a[i].id;
	}
	return s;
}

string strs(const array<string> &in a)
{
	string s = a.length() + ":[";
	for (uint i = 0; i < a.length(); i++)
	{
		if (i > 0) s += "|";
		s += a[i];
	}
	return s + "]";
}

uint rng_state = 12345;
uint rnd(uint n)
{
	rng_state = rng_state * 1103515245 + 12345;
	return (rng_state >> 8) % n;
}

array<Item@> makeItems(uint n, uint range)
{
	array<Item@> a;
	for (uint i = 0; i < n; i++)
		a.insertLast(Item(int(rnd(range)), int(i)));
	return a;
}

void testStrings()
{
	say("-- strings");
	string a = "abc";
	string b = a + "def";
	say(b);
	say(b + 5);
	say(5 + b);
	say(b + 2.5);
	say(2.5 + b);
	say(b + 0.1f);
	say(0.1f + b);
	say(b + true);
	say(false + b);
	say(b + uint(7));
	say(uint(8) + b);
	say(b + (-3));
	string c;
	c = 42; say(c);
	c = uint(4000000000); say(c);
	c = -17; say(c);
	c = 3.14159f; say(c);
	c = 1.0 / 3.0; say(c);
	c = 1e10; say(c);
	c = 1e-5; say(c);
	c = 100000.0f; say(c);
	c = 1234567.0f; say(c);
	c = 0.5; say(c);
	c = true; say(c);
	c = false; say(c);
	c += 1; say(c);
	c += uint(2); say(c);
	c += 1.5; say(c);
	c += 2.25f; say(c);
	c += true; say(c);
	c += "x"; say(c);
	string d = "hello";
	string e = d;
	e += "!";
	say(d + " " + e);
	string@ h = d;
	h += "?";
	say(d);
	say("len " + d.length());
	d.resize(3); say(d + "|" + d.length());
	d.resize(6); say(d.length());
	say(d[0] + " " + d[1] + " " + d[2] + " " + d[3]);
	d[0] = 72; say(d);
}

void testStringCompare()
{
	say("-- compare");
	string a = "apple", b = "apricot", e = "", z = "apple";
	say((a < b) + " " + (a > b) + " " + (a <= z) + " " + (a >= b) + " " + (a == z) + " " + (a != b));
	say(a.opCmp(b) + " " + b.opCmp(a) + " " + a.opCmp(z) + " " + e.opCmp(a) + " " + a.opCmp(e));
	string p = "ab", q = "abc";
	say(p.opCmp(q) + " " + q.opCmp(p));
	say((p == q) + " " + (e == ""));
	string hi; hi.resize(1); hi[0] = 200;
	string lo = "a";
	say(hi.opCmp(lo) + " " + lo.opCmp(hi));
}

void testStringErrors()
{
	say("-- string errors");
	string s = "abc";
	try { uint8 x = s[3]; say("read " + x); } catch { say("caught"); exc(); }
	try { s[10] = 1; say("wrote"); } catch { say("caught"); exc(); }
	say(s);
}

void testIntArray()
{
	say("-- int array");
	array<int> a = {5, 3, 9, 1, 3};
	say(ints(a));
	a.insertLast(7); say(ints(a));
	a.insertAt(0, 10); say(ints(a));
	a.insertAt(3, 11); say(ints(a));
	a.insertAt(a.length(), 12); say(ints(a));
	a.removeAt(1); say(ints(a));
	a.removeLast(); say(ints(a));
	a.removeRange(1, 2); say(ints(a));
	a.removeRange(2, 100); say(ints(a));
	a.removeRange(0, 0); say(ints(a));
	a.resize(6); say(ints(a));
	a.resize(2); say(ints(a));
	a.reserve(100); say(a.length());
	a.resize(0); say(ints(a) + " " + a.isEmpty());
	a = {8, 6, 4, 2, 9, 7, 5, 3, 1, 0};
	a.sortAsc(); say(ints(a));
	a.sortDesc(); say(ints(a));
	a.sortAsc(2, 5); say(ints(a));
	a.sortDesc(1, 3); say(ints(a));
	a.reverse(); say(ints(a));
	say(a.find(5) + " " + a.find(99) + " " + a.find(3, 5) + " " + a.find(0, 8));
	say(a.findByRef(a[2]));
	array<int> b(5);
	say(ints(b));
	array<int> c(3, 7);
	say(ints(c));
	b = c; say(ints(b));
	say((b == c) + " " + (b == a) + " " + (a == a));
	b.insertAt(1, c); say(ints(b));
	b.insertAt(b.length(), b); say(ints(b));
	array<int> s = {1, 2, 3, 4};
	s.insertAt(2, s); say(ints(s));
	s.insertAt(0, s); say(ints(s));
	array<int> big(100);
	for (uint i = 0; i < 100; i++) big[i] = int(rnd(1000)) - 500;
	big.sortAsc();
	bool sorted = true;
	for (uint i = 1; i < 100; i++) if (big[i - 1] > big[i]) sorted = false;
	say("sorted " + sorted + " " + big[0] + " " + big[99]);
	big.sortDesc(10, 80);
	sorted = true;
	for (uint i = 11; i < 90; i++) if (big[i - 1] < big[i]) sorted = false;
	say("desc range " + sorted);
}

void testPrimitives()
{
	say("-- primitives");
	array<float> f = {3.5f, -1.0f, 2.25f, 0.0f, 1e-3f};
	f.sortAsc(); say(flts(f));
	f.sortDesc(); say(flts(f));
	say(f.find(2.25f) + " " + f.find(2.5f));
	array<double> d = {3.5, -1.0, 2.25, 0.0, 1e-3};
	d.sortAsc(); say(d[0] + " " + d[4]);
	array<uint8> u = {200, 3, 255, 0, 17};
	u.sortAsc(); say(u[0] + " " + u[1] + " " + u[2] + " " + u[3] + " " + u[4]);
	array<int8> s8 = {-5, 100, -128, 127, 0};
	s8.sortAsc(); say(s8[0] + " " + s8[1] + " " + s8[2] + " " + s8[3] + " " + s8[4]);
	array<int16> s16 = {-5, 1000, -32768, 32767, 0};
	s16.sortDesc(); say(s16[0] + " " + s16[1] + " " + s16[2] + " " + s16[3] + " " + s16[4]);
	array<uint16> u16 = {5, 65535, 0, 300};
	u16.sortAsc(); say(u16[0] + " " + u16[1] + " " + u16[2] + " " + u16[3]);
	array<int64> i64 = {-5000000000, 5000000000, 0, 1};
	i64.sortAsc(); say(i64[0] + " " + i64[1] + " " + i64[2] + " " + i64[3]);
	say(i64.find(5000000000) + " " + i64.find(7));
	array<uint64> u64 = {5, 18446744073709551615, 0, 1};
	u64.sortAsc(); say(u64[0] + " " + u64[1] + " " + u64[2] + " " + u64[3]);
	array<uint> u32 = {5, 4294967295, 0, 1};
	u32.sortDesc(); say(u32[0] + " " + u32[1] + " " + u32[2] + " " + u32[3]);
	array<bool> bo = {true, false, true, false, false};
	bo.sortAsc(); say(bo[0] + " " + bo[1] + " " + bo[2] + " " + bo[3] + " " + bo[4]);
	say(bo.find(true));
	bo.reverse(); say(bo[0] + " " + bo[4]);
	bo.resize(7); say(bo.length() + " " + bo[6]);
}

void testStringArray()
{
	say("-- string array");
	array<string> a = {"pear", "apple", "fig", "banana", "apple"};
	say(strs(a));
	a.sortAsc(); say(strs(a));
	a.sortDesc(); say(strs(a));
	say(a.find("fig") + " " + a.find("nope") + " " + a.find(2, "apple"));
	a.insertLast("kiwi"); a.insertAt(1, "lime"); say(strs(a));
	a.removeAt(0); a.removeLast(); say(strs(a));
	a.resize(8); say(strs(a));
	a.resize(2); say(strs(a));
	array<string> b = a;
	b[0] += "!"; say(strs(a) + " " + strs(b));
	say((a == b) + " " + (a == a));
	array<string> c(3, "x"); say(strs(c));
	a.reverse(); say(strs(a));
	b.insertAt(1, c); say(strs(b));
	array<string@> h = {"b", "a", null, "c"};
	say(h.length() + " " + (h[2] is null) + " " + h[0]);
	h.sortAsc();
	string r;
	for (uint i = 0; i < h.length(); i++) r += (h[i] is null ? "null" : h[i]) + ",";
	say(r);
	h.sortDesc();
	r = "";
	for (uint i = 0; i < h.length(); i++) r += (h[i] is null ? "null" : h[i]) + ",";
	say(r);
	say(h.findByRef(h[1]));
}

void itemsBody(int before)
{
	{
		array<Item@> a = makeItems(5, 3);
		say(ids(a));
		a.sortAsc(); say(ids(a));
		a.sortDesc(); say(ids(a));
		array<Item@> b = makeItems(17, 4);
		b.sortAsc(); say(ids(b));
		array<Item@> c = makeItems(40, 6);
		c.sortAsc(); say(ids(c));
		c.sortDesc(); say(ids(c));
		c.sortAsc(5, 20); say(ids(c));
		array<Item@> d = makeItems(300, 10);
		d.sortAsc();
		string r;
		for (uint i = 0; i < d.length(); i++) r += d[i].id + " ";
		say(r);
		d.sortDesc();
		r = "";
		for (uint i = 0; i < d.length(); i++) r += d[i].id + " ";
		say(r);
		array<Item@> e = makeItems(2000, 5);
		e.sortAsc();
		int h = 0;
		for (uint i = 0; i < e.length(); i++) h = h * 31 + e[i].id;
		say("hash " + h);
		array<Item@> n = makeItems(6, 4);
		n.insertAt(2, null); n.insertLast(null); n.insertAt(0, null);
		say(ids(n));
		n.sortAsc(); say(ids(n));
		n.sortDesc(); say(ids(n));
		say(n.find(Item(2, 0)) + " " + n.find(Item(99, 0)));
		say(n.findByRef(n[3]) + " " + n.findByRef(null));
		array<Item@> g = makeItems(4, 100);
		g.removeAt(1); g.removeLast(); g.resize(10); g.resize(3);
		say(ids(g));
		say("live " + (live - before));
	}
}

void testItems()
{
	say("-- items");
	int before = live;
	itemsBody(before);
	say("live after " + (live - before));
}

void itemValuesBody(int before)
{
	{
		array<Item> a(4);
		for (uint i = 0; i < a.length(); i++) { a[i].k = int(3 - i); a[i].id = int(i); }
		string r;
		for (uint i = 0; i < a.length(); i++) r += a[i].k + "/" + a[i].id + " ";
		say(r);
		a.sortAsc();
		r = "";
		for (uint i = 0; i < a.length(); i++) r += a[i].k + "/" + a[i].id + " ";
		say(r);
		array<Item> b = a;
		b[0].k = 100;
		say(a[0].k + " " + b[0].k);
		a.insertLast(Item(9, 9)); a.insertAt(0, Item(8, 8));
		say(a.length() + " " + a[0].k + " " + a[5].k);
		say(a.find(Item(2, 0)) + " " + a.find(Item(77, 0)));
		say((a == b) + " " + (a == a));
		array<Item> c(5, Item(1, 2));
		say(c.length() + " " + c[4].k + "/" + c[4].id);
		a.removeRange(1, 3);
		say(a.length());
		say("live " + (live - before));
	}
}

void testItemValues()
{
	say("-- item values");
	int before = live;
	itemValuesBody(before);
	say("live after " + (live - before));
}

bool lessInt(const int &in a, const int &in b) { return a < b; }
bool greaterInt(const int &in a, const int &in b) { return a > b; }
bool lessItem(const Item@ const &in a, const Item@ const &in b) { return a.k < b.k; }
int cmpCalls = 0;
bool countingLess(const int &in a, const int &in b) { cmpCalls++; return a < b; }

void testCallbackSort()
{
	say("-- callback sort");
	array<int> a = {5, 3, 9, 1, 3, 8, 2, 7};
	a.sort(lessInt); say(ints(a));
	a.sort(greaterInt); say(ints(a));
	a.sort(lessInt, 2, 4); say(ints(a));
	a.sort(function(a, b) { return a > b; }, 1, 100); say(ints(a));
	a.sort(countingLess); say(ints(a) + " calls " + cmpCalls);
	array<int> one = {4};
	one.sort(lessInt); say(ints(one));
	array<Item@> b = makeItems(30, 5);
	b.sort(lessItem);
	say(ids(b));
	b.sort(function(a, b) { return a.k > b.k; }, 0, 10);
	say(ids(b));
	array<Item@> c = makeItems(8, 3);
	c.insertAt(3, null);
	try { c.sort(function(a, b) { return a.k < b.k; }); say(ids(c)); } catch { say("caught"); exc(); }
}

void testExceptions()
{
	say("-- exceptions");
	array<int> a = {1, 2, 3};
	try { say("v " + a[3]); } catch { say("caught"); exc(); }
	try { a.removeAt(3); } catch { say("caught"); exc(); }
	try { a.insertAt(4, 1); } catch { say("caught"); exc(); }
	try { a.removeRange(4, 1); } catch { say("caught"); exc(); }
	try { a.sortAsc(2, 5); } catch { say("caught"); exc(); }
	try { a.sortAsc(3, 2); } catch { say("caught"); exc(); }
	try { a.sort(lessInt, 3, 2); } catch { say("caught"); exc(); }
	try { a.resize(0xFFFFFFF0); } catch { say("caught"); exc(); }
	try { array<int> big(0xFFFFFFF0); say("made"); } catch { say("caught"); exc(); }
	try { a.insertAt(0, a); say(ints(a)); } catch { say("caught"); exc(); }
	array<int> e;
	try { e.removeLast(); } catch { say("caught"); exc(); }
	try { say("e " + e[0]); } catch { say("caught"); exc(); }
	say(ints(a));
	array<Plain> p(2);
	try { p.sortAsc(); } catch { say("caught"); exc(); }
	try { say("f " + p.find(Plain(1))); } catch { say("caught"); exc(); }
	array<Plain@> ph = {Plain(1), Plain(2)};
	try { ph.sortAsc(); } catch { say("caught"); exc(); }
}

void testNested()
{
	say("-- nested");
	array<array<int>> m = {{1, 2}, {3}, {}};
	say(m.length() + " " + m[0].length() + " " + m[1].length() + " " + m[2].length());
	m[2].insertLast(5); m.insertLast({9, 9, 9});
	string r;
	for (uint i = 0; i < m.length(); i++) r += ints(m[i]) + " ";
	say(r);
	m.resize(2); say(m.length());
	array<array<int>> n = m;
	n[0][0] = 77;
	say(m[0][0] + " " + n[0][0]);
	array<int>[] old(3);
	say(old.length());
	foreach (auto v, auto i : n) say(i + "=" + v.length());
	array<int> fe = {4, 5, 6};
	foreach (auto v : fe) say("v" + v);
}

void handlesBody(int before)
{
	{
		Item@ x = Item(1, 1);
		array<Item@> a = {x, x, null};
		say(a.length() + " " + a.findByRef(x) + " " + a.findByRef(x) + " " + a.find(2, x) + " " + a.findByRef(2, x));
		@a[2] = Item(5, 5);
		say(ids(a));
		a.resize(1);
		say(ids(a) + " live " + (live - before));
		array<Item@> b = a;
		a.removeAt(0);
		say(ids(b) + " live " + (live - before));
		b.removeLast();
		@x = null;
		say("live " + (live - before));
	}
}

void testHandles()
{
	say("-- handles");
	int before = live;
	handlesBody(before);
	say("live after " + (live - before));
	array<Plain@> ps = {Plain(1), Plain(2), Plain(3)};
	ps.insertAt(1, Plain(7));
	say(ps.length() + " " + ps[1].v);
	ps.reverse();
	say(ps[0].v + " " + ps[3].v);
}

void main()
{
	testStrings();
	testStringCompare();
	testStringErrors();
	testIntArray();
	testPrimitives();
	testStringArray();
	testItems();
	testItemValues();
	testCallbackSort();
	testExceptions();
	testNested();
	testHandles();
	say("end");
}
