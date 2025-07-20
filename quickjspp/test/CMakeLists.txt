macro(make_test name)
add_executable(${name} ${name}.cpp)
target_link_libraries(${name} quickjspp quickjs-dumpleaks)
add_test(${name} ${name})
# try to compile with -DCONFIG_CHECK_JSVALUE
add_library(${name}-checkjsv OBJECT ${name}.cpp)
target_compile_definitions(${name}-checkjsv PRIVATE CONFIG_CHECK_JSVALUE=1)
target_link_libraries(${name}-checkjsv quickjspp)
endmacro()

foreach(test
        value class exception example multicontext conversions point variant function_call inheritance jobs unhandled_rejection module_loader
        enum
        )
make_test(${test})
endforeach()

