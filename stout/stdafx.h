// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <stdexcept>

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

class stout_exception : public std::runtime_error
{
public:
    explicit stout_exception(const char* msg) : std::runtime_error(msg){;}
};


// TODO: reference additional headers your program requires here
