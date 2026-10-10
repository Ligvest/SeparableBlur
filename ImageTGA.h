#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace Image
{

struct Pixel24
{
	uint8_t B;
	uint8_t G;
	uint8_t R;
};

struct Pixel32
{
	uint8_t B;
	uint8_t G;
	uint8_t R;
	uint8_t Attribute;
};

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

std::vector<Pixel24> ReadImagePixels24FromFStream( std::ifstream& FileStream );
std::vector<Pixel32> ReadImagePixels32FromFStream( std::ifstream& FileStream );

class ImageTGA
{
public:
	// void ReadFromFile( const std::string& FileName );
	void PrintHeader() const;

	// Blurs the image with a separable Gaussian and saves it next to the input file with the "out_" prefix.
	// Sigma is in pixels, 0 means no blur
	void BlurAndSaveToFile( const std::string& InputFileName, const std::string& OutputFileName, float Sigma );

private:
	// Read from file methods
	void FillImageHeaderTGAFromFStream( std::ifstream& FileStream );
	void FillImageIdTGAFromFStream( std::ifstream& FileStream );

	// Write to file methods
	void WriteImageHeaderTGAToFStream( std::ofstream& FileStream ) const;
	void WriteImageIdTGAToFStream( std::ofstream& FileStream ) const;
	// Blurs pixel rows [FirstRow, EndRow) and writes them into their place in the output file.
	// Opens its own file streams, so several strips can be processed in parallel threads
	void BlurStripAndWriteToFile( const std::string& InputFileName, const std::string& OutputFileName, std::uint64_t PixelDataOffset, int FirstRow, int EndRow, const std::vector<float>& Kernel,
	                              int Radius ) const;

public:
	ImageHeaderTGA Header;

	// ImageId Field ( Located after header in the TGA 2.0 )
	std::vector<uint8_t> ImageId;
};

}  // namespace Image
