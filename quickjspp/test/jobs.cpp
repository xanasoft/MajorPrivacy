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
    context.global()["testFcn"] = [&called]() {
        called = true;
    };

    qjs::Value caller = context.eval(R"xxx(
        nextTick(testFcn);
    )xxx");

    assert(!called && "`testFcn` should not be called yet");

    while (runtime.isJobPending()) {
        runtime.executePendingJob();
    }

    assert(called && "`testFcn` should have been called");

    context.enqueueJob([](){
        throw std::runtime_error("some error");
    });

    try {
        while (runtime.isJobPending()) {
            runtime.executePendingJob();
        }
        assert(false && "Job execution should have failed");
    } catch (qjs::exception const & exc) {
        assert(&exc.context() == &context && "Exception should contain context information");
    }
    // Check that not executed jobs won't introduce memory leaks
    context.enqueueJob([](){
        assert(false && "This job will not be called");
    });
    return 0;
}
