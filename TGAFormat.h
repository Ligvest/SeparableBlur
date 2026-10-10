#pragma once

#include <cstdint>
#include <iosfwd>
#include <vector>

namespace Image
{

// Size is 20 because of padding. Intentionally kept the order of fields the same as in TGA 2.0 documentation
struct ImageHeaderTGA
{
	// The length of the ImageId field which is located after the header, and 0 here means, that no such field exists in the file
	// This ImageId field contains identifying information about the image.
	uint8_t IdLength;

	// Supported: only 0 ( no Color Map )
	uint8_t ColorMapType;

	// Supported: only 2 ( True color. No Compression )
	uint8_t ImageType;

	// Color Map Specification (5 Bytes)
	// Not Supported ( works only if ColorMapType not 0 )
	uint16_t FirstEntryIndex;
	uint16_t ColorMapLength;
	uint8_t ColorMapEntrySize;

	// Unused in modifying this image
	uint16_t XOriginOfImage;
	uint16_t YOriginOfImage;

	// Image Width in pixels
	uint16_t Width;
	// Image Height in pixels
	uint16_t Height;

	// Pixel Depth. Number of bits per pixel.
	// Supported: only 24 and 32
	uint8_t PixelDepth;

	// It consists of 8 bits:
	// bits 7,6 - reserved.
	// bits 5,4 - image origin:
	//	0,0 - bottom left
	//	0,1 - bottom right
	//	1,0 - top left
	//	1,1 - top right
	// bits 3-0 - this is a 4 bit number. This is the number of attribute bits in the pixel. Bits, which are not a color itself.
	uint8_t ImageDescriptor;  // To extract: attributeBits = imageDescriptor & 0x0F;
};

// Reads the 18-byte TGA Header field by field ( the ImageHeaderTGA struct itself is bigger because of padding )
ImageHeaderTGA ReadHeaderTGA( std::istream& Stream );
// Writes TGA Header field by field to stream ( 18-bytes in total )
void WriteHeaderTGA( std::ostream& Stream, const ImageHeaderTGA& Header );

// Reads the ImageId field ( stream pos should be just after the Header )
std::vector<uint8_t> ReadImageIdTGA( std::istream& Stream, const ImageHeaderTGA& Header );
// Writes ImageId field to stream ( stream pos should be just after the Header )
void WriteImageIdTGA( std::ostream& Stream, const std::vector<uint8_t>& ImageId );

// Checks that the image is of a kind we support: uncompressed true-color, 24 or 32 bits, not empty
void CheckHeaderTGA( const ImageHeaderTGA& Header );

// Utility function to print an image header
void PrintHeaderTGA( const ImageHeaderTGA& Header );

}  // namespace Image
