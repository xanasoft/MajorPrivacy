#include "quickjspp.hpp"
#include "quickjs/quickjs-libc.h"

#include <iostream>
#include <string_view>

static bool bignum_ext = false;

/* also used to initialize the worker context */
static JSContext *JS_NewCustomContext(JSRuntime *rt)
{
    JSContext *ctx;
    ctx = JS_NewContext(rt);
    if (!ctx)
        return NULL;
#ifdef CONFIG_BIGNUM
    if (bignum_ext) {
        JS_AddIntrinsicBigFloat(ctx);
        JS_AddIntrinsicBigDecimal(ctx);
        JS_AddIntrinsicOperators(ctx);
        JS_EnableBignumExt(ctx, true);
    }
#endif
    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");
    return ctx;
}

int main(int argc, char ** argv)
{
    qjs::Runtime runtime;
    auto rt = runtime.rt;

    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(rt);

    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);

    qjs::Context context(JS_NewCustomContext(rt));
    auto ctx = context.ctx;

    int flags = -1;
    int optind = 1;
    // load as ES6 module
    if(argv[optind] && (argv[optind] == std::string_view{"-m"} || argv[optind] == std::string_view{"--module"}))
    {
        flags = JS_EVAL_TYPE_MODULE;
        optind++;
    }
    // load as ES6 script
    else if(argv[optind] && argv[optind] == std::string_view{"--script"})
    {
        flags = JS_EVAL_TYPE_GLOBAL;
        optind++;
    }
    // enable bignum
    if(argv[optind] && argv[optind] == std::string_view{"--bignum"})
    {
        bignum_ext = true;
        optind++;
    }

    js_std_add_helpers(ctx, argc - optind, argv + optind);

    /* make 'std' and 'os' visible to non module code */
    context.eval(R"xxx(
        import * as std from 'std';
        import * as os from 'os';
        globalThis.std = std;
        globalThis.os = os;
    )xxx", "<input>", JS_EVAL_TYPE_MODULE);


    try
    {
        if(auto filename = argv[optind])
        {
            auto buf = qjs::detail::readFile(filename);
            if (!buf)
                throw std::runtime_error{std::string{"can't read file: "} + filename};

            // autodetect file type
            if(flags == -1)
                flags = JS_DetectModule(buf->data(), buf->size()) ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;

            context.eval(*buf, filename, flags);
        }
        else
        {
            std::cout << argv[0] << " [--module|--script] [--bignum] <filename>" << std::endl;
            js_std_free_handlers(rt);
            return 1;
        }
    }
    catch(qjs::exception & e)
    {
        auto exc = e.get();
        std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
        if((bool)exc["stack"])
            std::cerr << (std::string)exc["stack"] << std::endl;

        js_std_free_handlers(rt);
        return 1;
    }

    js_std_loop(ctx);

    js_std_free_handlers(rt);

    return 0;

}
