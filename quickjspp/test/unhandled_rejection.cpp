#include "quickjspp.hpp"
#include <iostream>
#include <stdexcept>

int main()
{
    qjs::Runtime runtime;
    qjs::Context context(runtime);
    
    context.global()["nextTick"] = [&context](std::function<void()> f) {
        context.enqueueJob(std::move(f));
    };

    bool called = false;
    context.onUnhandledPromiseRejection = [&called](qjs::Value reason) {
        called = true;
    };

    // Catch the rejection
    called = false;
    context.eval(R"xxx(
        (() => {
            const p = new Promise((resolve, reject) => {
                nextTick(() => {
                    reject(123);
                })
            });
            p.catch(() => {/* no-op */});
        })();
    )xxx");

    while (runtime.isJobPending()) {
        runtime.executePendingJob();
    }

    assert(!called && "Unhandled Promise rejection should not have been called");

    // Catch the rejection, but only one *after* it happens.
    called = false;
    context.eval(R"xxx(
        (() => {
            const p = new Promise((resolve, reject) => {
                reject(123);
                nextTick(() => {
                    p.catch(() => {/* no-op */});
                });
            });
        })();
    )xxx");

    while (runtime.isJobPending()) {
        runtime.executePendingJob();
    }

    assert(!called && "Unhandled Promise rejection should not have been called");

    // Do not catch the rejection
    called = false;
    context.eval(R"xxx(
        (() => {
            const p = new Promise((resolve, reject) => {
                reject(123);
                nextTick(() => {
                    p.then(() => {/* no-op */})
                });
            });
        })();
    )xxx");

    assert(!called && "Unhandled Promise rejection should not have been called yet");

    while (runtime.isJobPending()) {
        runtime.executePendingJob();
    }

    assert(called && "Unhandled Promise rejection should have been called");

    return 0;
}
