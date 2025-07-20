#include "quickjspp.hpp"
#include <iostream>

struct A {
    int a;
    A(int a) : a(a) {}
    virtual ~A() = default;
    virtual int vfunc_a1() {
        return a;
    }
    virtual int vfunc_a2() {
        return a + 1;
    }
    int non_vfunc_a1() {
        return a + 2;
    }
    int non_vfunc_a2() {
        return a + 3;
    }
};

struct B {
    int b;
    B(int b) : b(b) {}
    virtual ~B() = default;
    virtual int vfunc_b1() {
        return b;
    }
    virtual int vfunc_b2() {
        return b + 1;
    }
    int non_vfunc_b1() {
        return b + 2;
    }
    int non_vfunc_b2() {
        return b + 3;
    }
};

struct H {
    int h;
    H(int h) : h(h) {}
    virtual ~H() = default;
    virtual int vfunc_h1() {
        return h;
    }
    virtual int vfunc_h2() {
        return h + 1;
    }
    int non_vfunc_h1() {
        return h + 2;
    }
    int non_vfunc_h2() {
        return h + 3;
    }
};

struct C : public A, public B, public H {
    int c;
    C(int a, int b, int c, int h) : A(a), B(b), H(h), c(c) {}
    int vfunc_a1() override {
        return 42;
    }
    int vfunc_b2() override {
        return 84;
    }
    int non_vfunc_a1() {
        return -42;
    }
    int non_vfunc_b2() {
        return -84;
    }
    int non_vfunc_h1() {
        return -126;
    }
};



int main()
{
    qjs::Runtime runtime;
    qjs::Context context(runtime);

    auto& test = context.addModule("test");
    test.class_<A>("A")
        .constructor<int>()
        .fun<&A::a>("a")
        .fun<&A::vfunc_a1>("vfunc_a1")
        .fun<&A::vfunc_a2>("vfunc_a2")
        .fun<&A::non_vfunc_a1>("non_vfunc_a1")
        .fun<&A::non_vfunc_a2>("non_vfunc_a2");

    test.class_<B>("B")
        .constructor<int>()
        .fun<&B::b>("b")
        .fun<&B::vfunc_b1>("vfunc_b1")
        .fun<&B::vfunc_b2>("vfunc_b2")
        .fun<&B::non_vfunc_b1>("non_vfunc_b1")
        .fun<&B::non_vfunc_b2>("non_vfunc_b2");

    test.class_<C>("C")
        .constructor<int, int, int, int>()
        .base<A>()
        .fun<&C::c>("c")
        .fun<&C::b>("b")
        .fun<&C::h>("h")
        .fun<&C::vfunc_b1>("vfunc_b1")
        .fun<&C::vfunc_b2>("vfunc_b2")
        .fun<&C::vfunc_h1>("vfunc_h1")
        .fun<&C::vfunc_h2>("vfunc_h2")
        .fun<&C::non_vfunc_a1>("non_vfunc_a1")
        .fun<&C::non_vfunc_b1>("non_vfunc_b1")
        .fun<&C::non_vfunc_b2>("non_vfunc_b2")
        .fun<&C::non_vfunc_h1>("non_vfunc_h1")
        .fun<&C::non_vfunc_h2>("non_vfunc_h2");

    try {
        context.eval(R"xxx(
            import { A, B, C } from "test";

            function assert(b, str = "FAIL") {
                if (b) {
                    return;
                } else {
                    throw Error("assertion failed: " + str);
                }
            }

            function assert_eq(a, b, str = "") {
                assert(a === b, `${JSON.stringify(a)} should be equal to ${JSON.stringify(b)}. ${str}`);
            }

            function assert_throw(g, str = "") {
                try {
                    f();
                    assert(false, `Expression should have thrown`)
                } catch (e) {
                }
            }

            // A
            const a = new A(123);
            
            assert_eq(a.a, 123, "a.a == 123");
            assert_eq(a.vfunc_a1(), 123, "a.vfunc_a1() == 123");
            assert_eq(a.vfunc_a2(), 124, "a.vfunc_a2() == 124");
            assert_eq(a.non_vfunc_a1(), 125, "a.non_vfunc_a1() == 125");
            assert_eq(a.non_vfunc_a2(), 126, "a.non_vfunc_a2() == 126");

            // B
            const b = new B(123);
            
            assert_eq(b.b, 123, "b.b == 123");
            assert_eq(b.vfunc_b1(), 123, "b.vfunc_b1() == 123");
            assert_eq(b.vfunc_b2(), 124, "b.vfunc_b2() == 124");
            assert_eq(b.non_vfunc_b1(), 125, "b.non_vfunc_b1() == 125");
            assert_eq(b.non_vfunc_b2(), 126, "b.non_vfunc_b2() == 126");

            // These should throw because A can't be casted to B
            assert_throw(() => b.vfunc_b1.call(a), "b.vfunc_b1.call(a)");
            assert_throw(() => b.vfunc_b2.call(a), "b.vfunc_b2.call(a)");
            assert_throw(() => b.non_vfunc_b1.call(a), "b.non_vfunc_b1.call(a)");
            assert_throw(() => b.non_vfunc_b2.call(a), "b.non_vfunc_b2.call(a)");

            // C
            const c = new C(10, 20, 30, 40);

            assert_eq(c.c, 30, "c.c == 30");

            assert_eq(c instanceof A, true, "(c instanceof A) == true");
            assert_eq(c instanceof B, false, "(c instanceof B) == false");
            
            // Properties and methods inherited from A should work on C
            // These are the basic test
            assert_eq(c.a, 10, "c.a == 10");
            assert_eq(c.vfunc_a1(), 42, "c.vfunc_a1() == 42");
            assert_eq(c.vfunc_a2(), 11, "c.vfunc_a2() == 11");
            assert_eq(c.non_vfunc_a1(), -42, "c.non_vfunc_a1() == -42");
            assert_eq(c.non_vfunc_a2(), 13, "c.non_vfunc_a2() == 13");

            // Check the behaviour of using A's methods on a C object
            // These test the behavior of virtual and non virtual functions
            assert_eq(a.vfunc_a1.call(c), 42, "a.vfunc_a1.call(c) == 42");
            assert_eq(a.vfunc_a2.call(c), 11, "a.vfunc_a2.call(c) == 11");
            assert_eq(a.non_vfunc_a1.call(c), 12, "a.non_vfunc_a1.call(c) == 12");
            assert_eq(a.non_vfunc_a2.call(c), 13, "a.non_vfunc_a2.call(c) == 13");

            // Properties and methods inherited from B should work on C
            // The interest of these test cases is that B is not the first inherited class
            assert_eq(c.b, 20, "c.b == 20");
            assert_eq(c.vfunc_b1(), 20, "c.vfunc_b1() == 20");
            assert_eq(c.vfunc_b2(), 84, "c.vfunc_b2() == 84");
            assert_eq(c.non_vfunc_b1(), 22, "c.non_vfunc_b1() == 22");
            assert_eq(c.non_vfunc_b2(), -84, "c.non_vfunc_b2() == -84");

            // Check the behaviour of using B's methods on a C object
            // These test the behavior of virtual and non virtual functions
            assert_eq(b.vfunc_b1.call(c), 20, "b.vfunc_b1.call(c) == 20");
            assert_eq(b.vfunc_b2.call(c), 84, "b.vfunc_b2.call(c) == 84");
            assert_eq(b.non_vfunc_b1.call(c), 22, "b.non_vfunc_b1.call(c) == 22");
            assert_eq(b.non_vfunc_b2.call(c), 23, "b.non_vfunc_b2.call(c) == 23");

            // Properties and methods inherited from H should work on C
            // The interest of these test cases is that H is never registered with JS
            assert_eq(c.h, 40, "c.h == 40");
            assert_eq(c.vfunc_h1(), 40, "c.vfunc_h1() == 40");
            assert_eq(c.vfunc_h2(), 41, "c.vfunc_h2() == 41");
            assert_eq(c.non_vfunc_h1(), -126, "c.non_vfunc_h1() == -126");
            assert_eq(c.non_vfunc_h2(), 43, "c.non_vfunc_h2() == 43");

        )xxx", "<eval>", JS_EVAL_TYPE_MODULE);
    }
    catch(qjs::exception)
    {
        auto exc = context.getException();
        std::cerr << (std::string) exc << std::endl;
        if((bool) exc["stack"])
            std::cerr << (std::string) exc["stack"] << std::endl;
        return 1;
    }

    return 0;
}