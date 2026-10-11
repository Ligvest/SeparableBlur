#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace IO
{

// Reads Count bytes from Stream. Terminates the program with ErrorMessage if the stream ends earlier
std::vector<uint8_t> ReadBytes( std::istream& Stream, std::size_t Count, const char* ErrorMessage );

// Writes all the bytes to Stream. Write errors are checked by the caller after closing the stream
void WriteBytes( std::ostream& Stream, const std::vector<uint8_t>& Bytes );

}  // namespace IO
