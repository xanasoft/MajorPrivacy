#include "quickjspp.hpp"
#include <iostream>

class MyClass
{
public:
    MyClass() {}

    void print_context(qjs::Value value) { std::cout << "Context: " << value.ctx << '\n'; }
};

void println(const std::string& str) { std::cout << str << std::endl; }

static void glue(qjs::Context::Module& module)
{
    module.function<&println>("println");
    module.class_<MyClass>("MyClass")
            .constructor<>()
            .fun<&MyClass::print_context>("print_context");
}


int main()
{
    qjs::Runtime runtime;
    qjs::Context context1(runtime);
    qjs::Context context2(runtime);
    qjs::Value val = JS_UNDEFINED, mc = JS_UNDEFINED;
    try
    {
        // export classes as a module
        glue(context1.addModule("MyModule"));
        // evaluate js code
        context1.eval(R"xxx(
            import * as my from 'MyModule';
            globalThis.mc = new my.MyClass();
            mc.print_context(undefined);
        )xxx", "<eval>", JS_EVAL_TYPE_MODULE);

        context1.global()["val"] = context1.fromJSON("{\"a\":\"1\",\"b\":{\"2\":2}}");
        val = context1.eval("globalThis.val");
        assert(val["a"].as<int>() == 1);

        mc = context1.eval("mc");
    }
    catch(qjs::exception e)
    {
        auto exc = e.get();
        std::cerr << (std::string) exc << std::endl;
        if((bool) exc["stack"])
            std::cerr << (std::string) exc["stack"] << std::endl;
        return 1;
    }
    try
    {
        glue(context2.addModule("MyModule"));
        context2.eval("import * as my from 'MyModule'; " "\n"
                      "(new my.MyClass()).print_context(undefined);" "\n"
                      ,"<eval>", JS_EVAL_TYPE_MODULE
        );
        context2.global()["val"] = val;
        assert(val == context2.eval("globalThis.val"));

        context2.global()["mc"] = mc;
        // will print the first context even though we call it from second
        // see js_call_c_function
        context2.eval("mc.print_context(undefined)");
    }
    catch(qjs::exception e)
    {
        auto exc = e.get();
        std::cerr << (std::string) exc << std::endl;
        if((bool) exc["stack"])
            std::cerr << (std::string) exc["stack"] << std::endl;
        return 1;
    }
}