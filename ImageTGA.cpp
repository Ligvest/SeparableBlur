#include "ImageTGA.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

namespace Image
{

[[noreturn]] static void TerminateWithMessage( const char* TerminationMessage )
{
	std::cerr << TerminationMessage << '\n';
	std::terminate();
}

static void CheckHeader( const ImageHeaderTGA& Header )
{
	// Color map is not supported
	if ( Header.ColorMapType != 0 )
	{
		TerminateWithMessage( "File shouldn't have Color map" );
	}

	// Only uncompressed true-color images are supported
	if ( Header.ImageType != 2 )
	{
		TerminateWithMessage( "Image type should be 2 ( True Color )" );
	}

	// Only 24 and 32 bits per pixel are supported
	if ( Header.PixelDepth != 24 && Header.PixelDepth != 32 )
	{
		TerminateWithMessage( "Pixel depth can only be 24 or 32 bits" );
	}

	// Empty image
	if ( Header.Width == 0 || Header.Height == 0 )
	{
		TerminateWithMessage( "Width or height is 0" );
	}
}

// 1D Gaussian kernel. Weights are normalized to sum to 1, so the image doesn't get darker or brighter
static std::vector<float> MakeGaussianKernel( int Radius, float Sigma )
{
	// No Blur
	if ( Sigma <= 0.0f )
	{
		return { 1.0f };
	}

	// 1D Kernel. Radius of Pixels before the center, the same amount after the center and +1 pixel in the center
	std::vector<float> Kernel( static_cast<std::size_t>( 2 * Radius + 1 ) );

	// A sum to normalize kernel later
	float Sum = 0.0f;
	for ( int Offset = -Radius; Offset <= Radius; ++Offset )
	{
		// Using formula for calculating Separable Gaussian Blur weights
		const float Weight = std::exp( -static_cast<float>( Offset * Offset ) / ( 2.0f * Sigma * Sigma ) );

		// Just write results
		Kernel[Offset + Radius] = Weight;
		Sum += Weight;
	}

	// Normalization
	for ( float& Weight : Kernel )
	{
		Weight /= Sum;
	}

	return Kernel;
}

// Blurs one row along X. Neighbors outside the image are taken from the edge pixel ( clamp )
static void BlurRowHorizontally( const uint8_t* SrcRow, int ImageWidth, int ChannelsInPixel, const std::vector<float>& Kernel, std::vector<float>& PaddedRow, float* DstRow, int Radius )
{
	// Convert the row from uint8_t to float
	// Pad the row with Radius copies of the edge pixel on each side ( "clamp to edge" ), so the blur loop below needs no bounds checks
	PaddedRow.resize( static_cast<std::size_t>( ImageWidth + 2 * Radius ) * ChannelsInPixel );
	// From the (left edge - Radius) pos to the (right edge + Radius ) pos
	for ( int X = -Radius; X < ImageWidth + Radius; ++X )
	{
		// ClampedX is just clamped position to return the last pixel if we go over the edge of the image
		// We clamp it because our loop goes beyond the edges of the image
		const int ClampedX = std::min( std::max( X, 0 ), ImageWidth - 1 );

		for ( int CurChannel = 0; CurChannel < ChannelsInPixel; ++CurChannel )
		{
			const int SrcRowChannelPos = ClampedX * ChannelsInPixel + CurChannel;
			// (X + Radius) - because indices for PaddedRow should be from 0 to ImageWidth + (2 * Radius)
			const int PaddedRowChannelPos = ( X + Radius ) * ChannelsInPixel + CurChannel;
			PaddedRow[PaddedRowChannelPos] = SrcRow[SrcRowChannelPos];
		}
	}

	// Bluring
	for ( int X = 0; X < ImageWidth; ++X )
	{
		for ( int CurChannel = 0; CurChannel < ChannelsInPixel; ++CurChannel )
		{
			float Sum = 0.0f;
			// Kernel and PaddedRow have the same shift.
			// That's why we don't need to correct our position in PaddedRow with ( X + Radius )
			for ( int KernelIndex = 0; KernelIndex <= 2 * Radius; ++KernelIndex )
			{
				// Using PaddedRow to calculate blurred pixel channel
				Sum += Kernel[KernelIndex] * PaddedRow[( X + KernelIndex ) * ChannelsInPixel + CurChannel];
			}
			// Write the blurred pixel channel in the result Row container
			DstRow[X * ChannelsInPixel + CurChannel] = Sum;
		}
	}
}

void ImageTGA::FillImageHeaderTGAFromFStream( std::ifstream& FileStream )
{
	// Field 1 ( 1 Byte )
	FileStream.read( reinterpret_cast<char*>( &Header.IdLength ), sizeof( Header.IdLength ) );
	// Field 2 ( 1 Byte )
	FileStream.read( reinterpret_cast<char*>( &Header.ColorMapType ), sizeof( Header.ColorMapType ) );
	// Field 3 ( 1 Byte )
	FileStream.read( reinterpret_cast<char*>( &Header.ImageType ), sizeof( Header.ImageType ) );
	// Field 4 ( 5 Bytes ) Color Map Specification
	FileStream.read( reinterpret_cast<char*>( &Header.FirstEntryIndex ), sizeof( Header.FirstEntryIndex ) );
	FileStream.read( reinterpret_cast<char*>( &Header.ColorMapLength ), sizeof( Header.ColorMapLength ) );
	FileStream.read( reinterpret_cast<char*>( &Header.ColorMapEntrySize ), sizeof( Header.ColorMapEntrySize ) );
	// Field 5 ( 10 Bytes ) Image Specification
	FileStream.read( reinterpret_cast<char*>( &Header.XOriginOfImage ), sizeof( Header.XOriginOfImage ) );
	FileStream.read( reinterpret_cast<char*>( &Header.YOriginOfImage ), sizeof( Header.YOriginOfImage ) );
	FileStream.read( reinterpret_cast<char*>( &Header.Width ), sizeof( Header.Width ) );
	FileStream.read( reinterpret_cast<char*>( &Header.Height ), sizeof( Header.Height ) );
	FileStream.read( reinterpret_cast<char*>( &Header.PixelDepth ), sizeof( Header.PixelDepth ) );
	FileStream.read( reinterpret_cast<char*>( &Header.ImageDescriptor ), sizeof( Header.ImageDescriptor ) );
}

void ImageTGA::WriteImageHeaderTGAToFStream( std::ofstream& FileStream ) const
{
	// Same fields in the same order as in FillImageHeaderTGAFromFStream
	// Field 1 ( 1 Byte )
	FileStream.write( reinterpret_cast<const char*>( &Header.IdLength ), sizeof( Header.IdLength ) );
	// Field 2 ( 1 Byte )
	FileStream.write( reinterpret_cast<const char*>( &Header.ColorMapType ), sizeof( Header.ColorMapType ) );
	// Field 3 ( 1 Byte )
	FileStream.write( reinterpret_cast<const char*>( &Header.ImageType ), sizeof( Header.ImageType ) );
	// Field 4 ( 5 Bytes ) Color Map Specification
	FileStream.write( reinterpret_cast<const char*>( &Header.FirstEntryIndex ), sizeof( Header.FirstEntryIndex ) );
	FileStream.write( reinterpret_cast<const char*>( &Header.ColorMapLength ), sizeof( Header.ColorMapLength ) );
	FileStream.write( reinterpret_cast<const char*>( &Header.ColorMapEntrySize ), sizeof( Header.ColorMapEntrySize ) );
	// Field 5 ( 10 Bytes ) Image Specification
	FileStream.write( reinterpret_cast<const char*>( &Header.XOriginOfImage ), sizeof( Header.XOriginOfImage ) );
	FileStream.write( reinterpret_cast<const char*>( &Header.YOriginOfImage ), sizeof( Header.YOriginOfImage ) );
	FileStream.write( reinterpret_cast<const char*>( &Header.Width ), sizeof( Header.Width ) );
	FileStream.write( reinterpret_cast<const char*>( &Header.Height ), sizeof( Header.Height ) );
	FileStream.write( reinterpret_cast<const char*>( &Header.PixelDepth ), sizeof( Header.PixelDepth ) );
	FileStream.write( reinterpret_cast<const char*>( &Header.ImageDescriptor ), sizeof( Header.ImageDescriptor ) );
}

void ImageTGA::WriteImageIdTGAToFStream( std::ofstream& FileStream ) const
{
	FileStream.write( reinterpret_cast<const char*>( ImageId.data() ), static_cast<std::streamsize>( ImageId.size() ) );
}

void ImageTGA::BlurAndWritePixelsTGAToFStream( std::ifstream& InputStream, std::ofstream& OutputStream, float Sigma ) const
{
	const int ImageWidth = Header.Width;                                                    // Width in Pixels
	const int ImageHeight = Header.Height;                                                  // Height in Pixels
	const int ChannelsInPixel = Header.PixelDepth / 8;                                      // 3 ( BGR ) or 4 ( BGRA ). All channels are blurred the same way
	const size_t ChannelsInRow = static_cast<std::size_t>( ImageWidth ) * ChannelsInPixel;  // Amount of channels in an image row

	const int Radius = static_cast<int>( std::ceil( 3.0f * Sigma ) );       // Radius = Sigma*3 - Good compromise between quality and performance
	const std::vector<float> Kernel = MakeGaussianKernel( Radius, Sigma );  // Making Kernel out of Sigma and Radius

	// Ring buffer of horizontally blurred rows.
	// For vertical blur we need current row and rows from Y - Radius to Y + Radius
	// So RingSize is (2 * Radius + 1) for kernel rows + 1 central one
	// This is the amount we need at a time.
	// Row N is stored in slot N % RingSize
	const int RingSize = std::min( 2 * Radius + 1, ImageHeight );  // Ring size in pixel rows
	std::vector<float> RingBuffer( ChannelsInRow * RingSize );     // Each value is a pixel channel
	const auto GetRowFromRingBuffer = [&]( int Row ) { return RingBuffer.data() + ( Row % RingSize ) * ChannelsInRow; };

	std::vector<float> BlurredRow( ChannelsInRow );   // Store here our result row but with float type
	std::vector<uint8_t> OutputRow( ChannelsInRow );  // Our converted to uint8_t result row which we will write in the output file

	// Rows are processed in the file order. The kernel is symmetric, so it doesn't matter if the image is stored bottom-up or top-down
	int RowsRead = 0;
	std::vector<uint8_t> InputRow( ChannelsInRow );  // Container to read Row of pixels
	std::vector<float> PaddedRow;                    // Moved out of the BlurRowHorizontally to let each thread to have its own version and avoid data conflicts
	for ( int Y = 0; Y < ImageHeight; ++Y )
	{
		// We should read at least Radius + 1 rows to use them in VerticalBlur
		const int LastNeededRow = std::min( Y + Radius, ImageHeight - 1 );
		while ( RowsRead <= LastNeededRow )
		{
			// Read next row of pixels to InputRow
			InputStream.read( reinterpret_cast<char*>( InputRow.data() ), static_cast<std::streamsize>( ChannelsInRow ) );
			if ( !InputStream )
			{
				TerminateWithMessage( "File is too short to contain all pixels" );
			}

			// Blur InputRow and write result in RingBuffer
			BlurRowHorizontally( InputRow.data(), ImageWidth, ChannelsInPixel, Kernel, PaddedRow, GetRowFromRingBuffer( RowsRead ), Radius );
			++RowsRead;
		}

		// Blur vertically: weighted sum of the neighbor rows, clamped at the top and bottom edges
		// Each time we accumulate the result in BlurredRow, so we need to 0 it
		std::fill( BlurredRow.begin(), BlurredRow.end(), 0.0f );
		for ( int KernelIndex = -Radius; KernelIndex <= Radius; ++KernelIndex )
		{
			// ClampedSrcY is a clamped position shifted by Kernel position to return the last pixel if we go over the edge of the image
			// We clamp it because our loop goes beyond the edges of the image
			const int ClampedShiftedY = std::min( std::max( Y + KernelIndex, 0 ), ImageHeight - 1 );
			const float Weight = Kernel[KernelIndex + Radius];
			const float* SrcRow = GetRowFromRingBuffer( ClampedShiftedY );
			// Go through all the channels in a row and write them into according channel of the resulting row
			for ( std::size_t I = 0; I < ChannelsInRow; ++I )
			{
				BlurredRow[I] += Weight * SrcRow[I];
			}
		}

		// Convert back from float to uint8_t with rounding.
		for ( std::size_t I = 0; I < ChannelsInRow; ++I )
		{
			// +0.5f turns truncation of the cast into rounding to nearest ( without it the image gets darker by half a level on average )
			// min with 255.0f guards against float error rounding up to 256, which doesn't fit into uint8_t
			OutputRow[I] = static_cast<uint8_t>( std::min( BlurredRow[I] + 0.5f, 255.0f ) );
		}

		// Write a row to the result file
		OutputStream.write( reinterpret_cast<const char*>( OutputRow.data() ), static_cast<std::streamsize>( ChannelsInRow ) );
	}
}

void ImageTGA::FillImageIdTGAFromFStream( std::ifstream& FileStream )
{
	// Field 6 ( Header.IdLength Bytes ) ImageId field
	ImageId.resize( Header.IdLength );
	FileStream.read( reinterpret_cast<char*>( ImageId.data() ), Header.IdLength );
	if ( !FileStream )
	{
		TerminateWithMessage( "File is too short to contain the header and ImageId field" );
	}
}

void ImageTGA::BlurAndSaveToFile( const std::string& InputFileName, const std::string& OutputFileName, float Sigma )
{
	// Open the input file
	std::ifstream InputStream( InputFileName, std::ios::binary );
	if ( !InputStream.is_open() )
	{
		TerminateWithMessage( "Couldn't open the input file" );
	}

	// Read Header and ImageId field
	FillImageHeaderTGAFromFStream( InputStream );
	CheckHeader( Header );
	FillImageIdTGAFromFStream( InputStream );
	// Next field contains pixels

	// Create the output file
	std::ofstream OutputStream( OutputFileName, std::ios::binary );
	if ( !OutputStream.is_open() )
	{
		TerminateWithMessage( "Couldn't create the output file" );
	}

	//  Write Header and ImageId field
	WriteImageHeaderTGAToFStream( OutputStream );
	WriteImageIdTGAToFStream( OutputStream );
	// Next field will contain pixels

	// Apply Separable Gaussian Blur to pixels, continuously reading them from InputStream and writing to OutputStream
	BlurAndWritePixelsTGAToFStream( InputStream, OutputStream, Sigma );

	// Copy everything after the pixels ( developer area, extension area, footer ) as is.
	// Pixel data keeps its size, so the offsets stored in the footer stay valid.
	// The check is needed because operator<< sets failbit when there is nothing to copy
	if ( InputStream.peek() != std::ifstream::traits_type::eof() )
	{
		OutputStream << InputStream.rdbuf();
	}

	OutputStream.close();
	if ( !OutputStream )
	{
		TerminateWithMessage( "Couldn't write the output file" );
	}
}

static const char* ImageTypeToStringTGA( uint8_t ImageType )
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
}

void ImageTGA::PrintHeader() const
{
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
	          << "  ImageType:         " << AsNumber( Header.ImageType ) << " (" << ImageTypeToStringTGA( Header.ImageType ) << ")\n"
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
