#pragma once

#include <exception>
#include <iostream>

namespace Error
{

// Prints the message to stderr and terminates the program
[[noreturn]] inline void TerminateWithMessage( const char* TerminationMessage )
{
	std::cerr << TerminationMessage << '\n';
	std::terminate();
}

}  // namespace Error
