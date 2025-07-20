#include "quickjspp.hpp"
#include <iostream>
#include <string_view>

int main()
{
    qjs::Runtime runtime;
    qjs::Context context(runtime);

    std::unordered_map<std::string_view, std::string> files = {
        {
            "some_module.js",
            R"xxx(
                import "folder/file1.js"
                log(import.meta.url);
            )xxx"
        },
        {
            "folder/file1.js",
            R"xxx(
                import "./file2.js"
                log(import.meta.url);
            )xxx"
        },
        {
            "folder/file2.js",
            R"xxx(
                import "http://localhost/script1.js";
                log(import.meta.url);
            )xxx"
        },
        {
            "http://localhost/script1.js",
            R"xxx(
                import "./script2.js";
                log(import.meta.url);
            )xxx"
        },
        {
            "http://localhost/script2.js",
            R"xxx(
                log(import.meta.url);
            )xxx"
        },
    };

    auto mock_module_loader = [&files](std::string_view filename) -> qjs::Context::ModuleData {
        if (files.count(filename)) return { qjs::detail::toUri(filename), files.at(filename) };
        return {};
    };

    // Test a failing import with the default moduleLoader
    try
    {
        context.eval(R"xxx(
            import "./invalid_module.js";
        )xxx", "<eval>", JS_EVAL_TYPE_MODULE);
        assert(false && "module import should have failed");
    }
    catch(qjs::exception e)
    {
        auto exc = e.get();
        assert(exc.isError() && "Exception should be a JS error");
        assert((std::string)exc == "ReferenceError: could not load module filename 'invalid_module.js'");
    }

    context.moduleLoader = mock_module_loader;
    context.global()["log"] = [](std::string_view s) {
        std::cout << s << std::endl;
    };

    // Test a successful import with the mock moduleLoader
    // It pretends to import things from the filesystem and from URLs
    context.eval(R"xxx(
        import "./some_module.js";
    )xxx", "<eval>", JS_EVAL_TYPE_MODULE);

    // Test a failing import with the mock moduleLoader
    try
    {
        context.eval(R"xxx(
            import "./invalid_module.js";
        )xxx", "<eval>", JS_EVAL_TYPE_MODULE);
        assert(false && "module import should have failed");
    }
    catch(qjs::exception e)
    {
        auto exc = e.get();
        assert(exc.isError() && "Exception should be a JS error");
        assert((std::string)exc == "ReferenceError: could not load module filename 'invalid_module.js'");
    }

    return 0;
}
