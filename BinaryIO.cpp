#include "BinaryIO.h"

#include <istream>
#include <ostream>

#include "Error.h"

namespace IO
{

std::vector<uint8_t> ReadBytes( std::istream& Stream, std::size_t Count, const char* ErrorMessage )
{
	std::vector<uint8_t> Bytes( Count );
	Stream.read( reinterpret_cast<char*>( Bytes.data() ), static_cast<std::streamsize>( Count ) );
	if ( !Stream )
	{
		Error::TerminateWithMessage( ErrorMessage );
	}
	return Bytes;
}

void WriteBytes( std::ostream& Stream, const std::vector<uint8_t>& Bytes )
{
	Stream.write( reinterpret_cast<const char*>( Bytes.data() ), static_cast<std::streamsize>( Bytes.size() ) );
}

}  // namespace IO
