#include <iostream>
#include <stdio.h>
#include "quickjspp.hpp"

enum TestEnum : int64_t{
    TestEnum_A,
    TestEnum_B = (1ll << 51),
    TestEnum_C,
};

class TestClass {
public:
    TestEnum e = TestEnum_C;

    TestEnum getE() {
        return e;
    }
};

void println(qjs::rest<std::string> args) {
    for (auto const &arg: args) std::cout << arg << " ";
    std::cout << "\n";
}

int main() {
    qjs::Runtime runtime;
    qjs::Context context(runtime);
    try {
        auto &module = context.addModule("MyModule");
        module.function<&println>("println");
        module.class_<TestClass>("TestClass")
                .constructor<>()
                .fun<&TestClass::getE>("getE");
        // import module
        context.eval(R"xxx(
            import * as my from 'MyModule';
            globalThis.my = my;
        )xxx",
                     "<import>", JS_EVAL_TYPE_MODULE);
        context.eval(R"xxx(
            let v1 = new my.TestClass();
            let e = v1.getE();
            my.println("enum value "+e);
        )xxx");
        assert(context.eval("e").as<TestEnum>() == TestEnum_C);
    } catch (qjs::exception) {
        auto exc = context.getException();
        std::cerr << (std::string) exc << std::endl;
        if ((bool) exc["stack"]) std::cerr << (std::string) exc["stack"] << std::endl;
        return 1;
    }
}
