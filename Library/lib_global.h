#pragma once

#ifndef BUILD_STATIC
# ifdef BUILD_DLL
#  define LIBRARY_EXPORT __declspec(dllexport)
# else
#  define LIBRARY_EXPORT __declspec(dllimport)
# endif
#else
# define LIBRARY_EXPORT
#endif

