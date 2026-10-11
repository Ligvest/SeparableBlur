#include "TGAFormat.h"

#include <iostream>
#include "Error.h"

namespace Image
{

ImageHeaderTGA ReadHeaderTGA( std::istream& Stream )
{
	ImageHeaderTGA Header{};

	// Field 1 ( 1 Byte )
	Stream.read( reinterpret_cast<char*>( &Header.IdLength ), sizeof( Header.IdLength ) );
	// Field 2 ( 1 Byte )
	Stream.read( reinterpret_cast<char*>( &Header.ColorMapType ), sizeof( Header.ColorMapType ) );
	// Field 3 ( 1 Byte )
	Stream.read( reinterpret_cast<char*>( &Header.ImageType ), sizeof( Header.ImageType ) );
	// Field 4 ( 5 Bytes ) Color Map Specification
	Stream.read( reinterpret_cast<char*>( &Header.FirstEntryIndex ), sizeof( Header.FirstEntryIndex ) );
	Stream.read( reinterpret_cast<char*>( &Header.ColorMapLength ), sizeof( Header.ColorMapLength ) );
	Stream.read( reinterpret_cast<char*>( &Header.ColorMapEntrySize ), sizeof( Header.ColorMapEntrySize ) );
	// Field 5 ( 10 Bytes ) Image Specification
	Stream.read( reinterpret_cast<char*>( &Header.XOriginOfImage ), sizeof( Header.XOriginOfImage ) );
	Stream.read( reinterpret_cast<char*>( &Header.YOriginOfImage ), sizeof( Header.YOriginOfImage ) );
	Stream.read( reinterpret_cast<char*>( &Header.Width ), sizeof( Header.Width ) );
	Stream.read( reinterpret_cast<char*>( &Header.Height ), sizeof( Header.Height ) );
	Stream.read( reinterpret_cast<char*>( &Header.PixelDepth ), sizeof( Header.PixelDepth ) );
	Stream.read( reinterpret_cast<char*>( &Header.ImageDescriptor ), sizeof( Header.ImageDescriptor ) );

	return Header;
}

void WriteHeaderTGA( std::ostream& Stream, const ImageHeaderTGA& Header )
{
	// Field 1 ( 1 Byte )
	Stream.write( reinterpret_cast<const char*>( &Header.IdLength ), sizeof( Header.IdLength ) );
	// Field 2 ( 1 Byte )
	Stream.write( reinterpret_cast<const char*>( &Header.ColorMapType ), sizeof( Header.ColorMapType ) );
	// Field 3 ( 1 Byte )
	Stream.write( reinterpret_cast<const char*>( &Header.ImageType ), sizeof( Header.ImageType ) );
	// Field 4 ( 5 Bytes ) Color Map Specification
	Stream.write( reinterpret_cast<const char*>( &Header.FirstEntryIndex ), sizeof( Header.FirstEntryIndex ) );
	Stream.write( reinterpret_cast<const char*>( &Header.ColorMapLength ), sizeof( Header.ColorMapLength ) );
	Stream.write( reinterpret_cast<const char*>( &Header.ColorMapEntrySize ), sizeof( Header.ColorMapEntrySize ) );
	// Field 5 ( 10 Bytes ) Image Specification
	Stream.write( reinterpret_cast<const char*>( &Header.XOriginOfImage ), sizeof( Header.XOriginOfImage ) );
	Stream.write( reinterpret_cast<const char*>( &Header.YOriginOfImage ), sizeof( Header.YOriginOfImage ) );
	Stream.write( reinterpret_cast<const char*>( &Header.Width ), sizeof( Header.Width ) );
	Stream.write( reinterpret_cast<const char*>( &Header.Height ), sizeof( Header.Height ) );
	Stream.write( reinterpret_cast<const char*>( &Header.PixelDepth ), sizeof( Header.PixelDepth ) );
	Stream.write( reinterpret_cast<const char*>( &Header.ImageDescriptor ), sizeof( Header.ImageDescriptor ) );
}

void CheckHeaderTGA( const ImageHeaderTGA& Header )
{
	// Color map is not supported
	if ( Header.ColorMapType != 0 )
	{
		Error::TerminateWithMessage( "File shouldn't have Color map" );
	}

	// Only uncompressed true-color images are supported
	if ( Header.ImageType != 2 )
	{
		Error::TerminateWithMessage( "Image type should be 2 ( True Color )" );
	}

	// Only 24 and 32 bits per pixel are supported
	if ( Header.PixelDepth != 24 && Header.PixelDepth != 32 )
	{
		Error::TerminateWithMessage( "Pixel depth can only be 24 or 32 bits" );
	}

	// Empty image
	if ( Header.Width == 0 || Header.Height == 0 )
	{
		Error::TerminateWithMessage( "Width or height is 0" );
	}
}

void PrintHeaderTGA( const ImageHeaderTGA& Header )
{
	auto ImageTypeToString = []( uint8_t ImageType ) -> const char*
	{
		switch ( ImageType )
		{
			case 0:
				return "No image data";
			case 1:
				return "Color-mapped";
			case 2:
				return "True-color";
			case 3:
				return "Grayscale";
			case 9:
				return "Color-mapped, RLE";
			case 10:
				return "True-color, RLE";
			case 11:
				return "Grayscale, RLE";
			default:
				return "Unknown";
		}
	};

	// uint8_t is a character type for std::ostream, so 8-bit fields go through unsigned to be printed as numbers, not as chars
	const auto AsNumber = []( uint8_t Value ) { return static_cast<unsigned int>( Value ); };

	// Bits 0-3
	const unsigned int AttributeBits = Header.ImageDescriptor & 0x0F;
	// Bit 4 ( we count bits from 0. Not from 1 )
	const bool bRightToLeft = ( Header.ImageDescriptor & 0x10 ) != 0;
	// Bit 5 ( we count bits from 0. Not from 1 )
	const bool bTopToBottom = ( Header.ImageDescriptor & 0x20 ) != 0;

	std::cout << "TGA header:\n"
	          << "  IdLength:          " << AsNumber( Header.IdLength ) << '\n'
	          << "  ColorMapType:      " << AsNumber( Header.ColorMapType ) << '\n'
	          << "  ImageType:         " << AsNumber( Header.ImageType ) << " (" << ImageTypeToString( Header.ImageType ) << ")\n"
	          << "  FirstEntryIndex:   " << Header.FirstEntryIndex << '\n'
	          << "  ColorMapLength:    " << Header.ColorMapLength << '\n'
	          << "  ColorMapEntrySize: " << AsNumber( Header.ColorMapEntrySize ) << '\n'
	          << "  XOriginOfImage:    " << Header.XOriginOfImage << '\n'
	          << "  YOriginOfImage:    " << Header.YOriginOfImage << '\n'
	          << "  Width:             " << Header.Width << '\n'
	          << "  Height:            " << Header.Height << '\n'
	          << "  PixelDepth:        " << AsNumber( Header.PixelDepth ) << '\n'
	          << "  ImageDescriptor:   " << AsNumber( Header.ImageDescriptor ) << " (origin: " << ( bTopToBottom ? "top" : "bottom" ) << '-' << ( bRightToLeft ? "right" : "left" )
	          << ", attribute bits: " << AttributeBits << ")\n";
}

}  // namespace Image
