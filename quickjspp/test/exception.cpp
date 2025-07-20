#include "quickjspp.hpp"
#include <iostream>
#include <stdexcept>

struct A{};

struct B {
    B() {
        throw std::runtime_error("constructor error");
    }
    B(int) {}
    void a_method() {
        throw std::runtime_error("method error");
    }
};

int main()
{
    qjs::Runtime runtime;
    qjs::Context context(runtime);
    context.global()["println"] = [](const std::string& s) { std::cout << s << std::endl; };

    context.registerClass<A>("A");
    context.registerClass<A>("A");

    auto& testMod = context.addModule("test");

    testMod.class_<B>("B")
        .constructor<>("B")
        .constructor<int>("B_int")
        .fun<&B::a_method>("a_method");

    try
    {
        auto obj = context.eval("Symbol.toPrimitive");
        std::cout << static_cast<int>(obj) << std::endl;
        assert(false);
    }
    catch(qjs::exception)
    {
        auto exc = context.getException();
        std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
        if((bool)exc["stack"])
            std::cerr << (std::string)exc["stack"] << std::endl;
        assert(exc.isError() && (std::string) exc == "TypeError: cannot convert symbol to number");
    }

    try
    {
        auto f = (std::function<void ()>) context.eval("(function() { +Symbol.toPrimitive })");
        f();
        assert(false);
    }
    catch(qjs::exception)
    {
        auto exc = context.getException();
        std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
        if((bool)exc["stack"])
            std::cerr << (std::string)exc["stack"] << std::endl;
        assert(exc.isError() && (std::string) exc == "TypeError: cannot convert symbol to number");
    }

    try
    {
        qjs::Value function = context.eval("() => { let a = b; }", "<test>");
        auto native = function.as<std::function<qjs::Value()>>();
        qjs::Value result = native();
        assert(false);
    }
    catch(qjs::exception)
    {
        auto exc = context.getException();
        std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
        if((bool)exc["stack"])
            std::cerr << (std::string)exc["stack"] << std::endl;
        assert(exc.isError() && (std::string) exc == "ReferenceError: 'b' is not defined");
    }

    // test the `wrap_call` case
    try
    {
        qjs::Value caller = context.eval("(f) => f();", "<test>");
        caller.as<std::function<void(std::function<void()>)>>()([](){
            throw std::runtime_error("some error");
        });
        assert(false);
    }
    catch(qjs::exception)
    {
        auto exc = context.getException();
        std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
        if((bool)exc["stack"])
            std::cerr << (std::string)exc["stack"] << std::endl;
        assert(exc.isError() && (std::string) exc == "InternalError: some error");
    }

    // test the `ctor_wrapper` case
    try
    {
        qjs::Value caller = context.eval(R"xxx(
            import { B } from "test";
            const b = new B();
        )xxx", "<eval>", JS_EVAL_TYPE_MODULE);
        assert(false);
    }
    catch(qjs::exception)
    {
        auto exc = context.getException();
        std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
        if((bool)exc["stack"])
            std::cerr << (std::string)exc["stack"] << std::endl;
        assert(exc.isError() && (std::string) exc == "InternalError: constructor error");
    }

    // test the `wrap_this_call` case
    try
    {
        qjs::Value caller = context.eval(R"xxx(
            import { B_int } from "test";
            const b = new B_int(123);
            b.a_method();
        )xxx", "<eval>", JS_EVAL_TYPE_MODULE);
        assert(false);
    }
    catch(qjs::exception)
    {
        auto exc = context.getException();
        std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
        if((bool)exc["stack"])
            std::cerr << (std::string)exc["stack"] << std::endl;
        assert(exc.isError() && (std::string) exc == "InternalError: method error");
    }
}
