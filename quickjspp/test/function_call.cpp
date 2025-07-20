#include "quickjs/quickjs.h"
#include "quickjspp.hpp"
#include <iostream>
#include <string_view>

int test_not_enough_arguments(qjs::Context & ctx) {
    std::string msg;

    ctx.global()["test_fcn"] = [](int a, int b, int c) {
        return a + b + c;
    };

    try
    {
        ctx.eval(R"xxx(
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

            try {
                test_fcn(1);
                assert(false);
            } catch (err) {
                assert(err instanceof TypeError); 
                assert_eq(err.message, 'Expected at least 3 arguments but received 1');
            }
        )xxx");
    }
    catch(qjs::exception)
    {
        auto exc = ctx.getException();
        std::cerr << (std::string) exc << std::endl;
        if((bool) exc["stack"])
            std::cerr << (std::string) exc["stack"] << std::endl;
        return 1;
    }

    return 0;
}

int test_call_with_rest_parameters(qjs::Context & ctx) {
    ctx.global()["test_fcn_rest"] = [](int a, qjs::rest<int> args) {
        for (auto arg : args) {
            a += arg;
        }
        return a;
    };

    ctx.global()["test_fcn_vec"] = [](int a, std::vector<int> args) {
        for (auto arg : args) {
            a += arg;
        }
        return a;
    };

    try
    {
        ctx.eval(R"xxx(
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

            assert_eq(test_fcn_rest(1, 2, 3, 4), 10);
            assert_eq(test_fcn_vec(1, [2, 3, 4]), 10);
            
            assert_throw(() => test_fcn_rest(1, [2, 3, 4]));
            assert_throw(() => test_fcn_vec(1, 2, 3, 4));
        )xxx");
    }
    catch(qjs::exception)
    {
        auto exc = ctx.getException();
        std::cerr << (std::string) exc << std::endl;
        if((bool) exc["stack"])
            std::cerr << (std::string) exc["stack"] << std::endl;
        return 1;
    }

    return 0;
}

int main()
{
    qjs::Runtime runtime;
    qjs::Context context(runtime);

    int ret = 0;
    ret |= test_not_enough_arguments(context);
    ret |= test_call_with_rest_parameters(context);

    return ret;
}