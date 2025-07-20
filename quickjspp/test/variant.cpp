#include "quickjspp.hpp"
#include <stdexcept>
#include <variant>
#include <iostream>

struct A {};

struct B {};

using var = std::variant<bool, int, double, std::string, std::vector<int>, std::shared_ptr<A>, std::shared_ptr<B>>;


auto f(var v1, const var& /*v2*/) -> var
{
    return std::visit([](auto&& v) -> var {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<A>>)
            return std::make_shared<B>();
        else if constexpr (std::is_same_v<T, std::shared_ptr<B>>)
            return std::make_shared<A>();
        else if constexpr (std::is_same_v<T, std::vector<int>>)
        {
            v.push_back(0);
            return v;
        } else
            return v + v;
    }, v1);
}

using var2 = std::variant<var, std::vector<std::pair<var, var>>>;


auto f2(var2 v2) -> var
{
    if(auto *v = std::get_if<var>(&v2))
        return *v;
    if(auto *vec = std::get_if<std::vector<std::pair<var, var>>>(&v2))
        return (*vec)[0].first;
    std::abort();
}

void assert_(bool condition)
{
    if(!condition) throw std::runtime_error("assertion failed");
}

static void qjs_glue(qjs::Context::Module& m)
{
    m.class_<::A>("A")
            .constructor<>()
        // implicit: .constructor<::A const &>()
            ;

    m.class_<::B>("B")
            .constructor<>()
        // implicit: .constructor<::B const &>()
            ;
    m.function<&::assert_>("assert"); // (bool)
    m.function<&::f>("f"); // (::var, ::var const &)
    m.function<&::f2>("f2");
} // qjs_glue


int main()
{
    qjs::Runtime runtime;
    qjs::Context context(runtime);

    try
    {
        // export classes as a module
        auto& module = context.addModule("MyModule");
        qjs_glue(module);
        // import module
        context.eval("import * as my from 'MyModule'; globalThis.my = my;", "<import>", JS_EVAL_TYPE_MODULE);
        // evaluate js code
        context.eval("let x = 1;"
                     "my.assert(my.f(x, x) === 2);"
                     "x = 'aaa';"
                     "my.assert(my.f(x, x) === 'aaaaaa');"
                     "x = 3.14;"
                     "my.assert(my.f(x, x) === 6.28);"
                     "x = [3,2,1];"
                     "my.assert(JSON.stringify(my.f(x, x)) === '[3,2,1,0]');"
                     "x = new my.A();"
                     "my.assert(my.f(x, x) instanceof my.B);"
                     "x = new my.B();"
                     "my.assert(my.f(x, x) instanceof my.A);"
                     "my.assert(my.f2(x) instanceof my.B);"
                     "my.assert(my.f2([[x,x]]) instanceof my.B);"
                     "my.assert(my.f2([[my.f(x,x),x]]) instanceof my.A);"
        );
    }
    catch(qjs::exception)
    {
        auto exc = context.getException();
        std::cerr << (std::string) exc << std::endl;
        if((bool) exc["stack"])
            std::cerr << (std::string) exc["stack"] << std::endl;
        return 1;
    }

}