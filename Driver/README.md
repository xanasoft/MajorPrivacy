# KernelIsolator
The Driver is not yet ready for prime time, also I may want to use it in a commertial project...

The driver is to a large part written in C++ and uses the custom a custom framework located at ..\Framework the framework implements smart pointers, and variouse usefull container types, obtimized for use in kernel mode.
It does not require a C++ runtime or exception support.

The driver also utilized ..\Library\Common\Variant.cpp/h for its communication with the user mode components, the CVariant implements a versatile yet fast mechanism to serialize data.

The driver uses parts of the MIT licenses System Informer source code.