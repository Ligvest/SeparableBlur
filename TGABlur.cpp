#include "TGABlur.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#include "BinaryIO.h"
#include "Error.h"
#include "GaussianBlur.h"
#include "TGAFormat.h"

namespace Image
{

// Blurs pixel rows [FirstRow, EndRow) and writes them into their place in the output file.
// Pixels is the whole pixel data field, shared read-only between the threads.
// Opens its own output stream, so several strips can be processed in parallel threads
static void BlurStripAndWriteToFile( const std::vector<uint8_t>& Pixels, const std::string& OutputFileName, const ImageHeaderTGA& Header, std::uint64_t PixelDataOffset, int FirstRow, int EndRow,
                                     const std::vector<float>& Kernel, int Radius )
{
	const int ImageWidth = Header.Width;                                                    // Width in Pixels
	const int ImageHeight = Header.Height;                                                  // Height in Pixels
	const int ChannelsInPixel = Header.PixelDepth / 8;                                      // 3 ( BGR ) or 4 ( BGRA ). All channels are blurred the same way
	const size_t ChannelsInRow = static_cast<std::size_t>( ImageWidth ) * ChannelsInPixel;  // Amount of channels in an image row

	// in | out opens the existing file without truncating it, so the strips written by other threads stay intact
	std::fstream OutputStream( OutputFileName, std::ios::in | std::ios::out | std::ios::binary );
	if ( !OutputStream.is_open() )
	{
		TerminateWithMessage( "Couldn't open the output file" );
	}
	OutputStream.seekp( static_cast<std::streamoff>( PixelDataOffset + FirstRow * ChannelsInRow ) );

	std::vector<float> VerticalRow( ChannelsInRow );               // Result of the vertical blur, the input for the horizontal one
	std::vector<const uint8_t*> SrcNeighborRows( Kernel.size() );  // Pointers to the 2 * Radius + 1 source rows the vertical blur needs for the current row

	std::vector<float> BlurredRow( ChannelsInRow );   // Store here our result row but with float type
	std::vector<uint8_t> OutputRow( ChannelsInRow );  // Our converted to uint8_t result row which we will write in the output file

	// Rows are processed in the file order. The kernel is symmetric, so it doesn't matter if the image is stored bottom-up or top-down
	std::vector<float> PaddedRow;  // Moved out of the BlurRowHorizontally to let each thread to have its own version and avoid data conflicts
	for ( int Y = FirstRow; Y < EndRow; ++Y )
	{
		// Collect the neighbor rows for the vertical blur, clamped at the top and bottom edges.
		// Rows of other strips are read too, which is safe because nobody writes into Pixels
		for ( int Offset = -Radius; Offset <= Radius; ++Offset )
		{
			// ClampedShiftedY is the row at Offset from Y, clamped to return the edge row if we go over the edge of the image
			// We clamp it because our loop goes beyond the edges of the image
			const int ClampedShiftedY = std::min( std::max( Y + Offset, 0 ), ImageHeight - 1 );
			SrcNeighborRows[Offset + Radius] = Pixels.data() + ClampedShiftedY * ChannelsInRow;
		}

		// Blur vertically: weighted sum of the neighbor rows
		BlurRowVertically( SrcNeighborRows.data(), ChannelsInRow, Kernel, VerticalRow.data() );

		// Blur the result of vertical blur horizontally. Each row of the strip is blurred horizontally exactly once
		BlurRowHorizontally( VerticalRow.data(), ImageWidth, ChannelsInPixel, Kernel, PaddedRow, BlurredRow.data(), Radius );

		// Convert back from float to uint8_t with rounding.
		for ( std::size_t I = 0; I < ChannelsInRow; ++I )
		{
			// +0.5f turns truncation of the cast into rounding to nearest ( without it the image gets darker by half a level on average )
			// min with 255.0f guards against float error rounding up to 256, which doesn't fit into uint8_t
			OutputRow[I] = static_cast<uint8_t>( std::min( BlurredRow[I] + 0.5f, 255.0f ) );
		}

		// Write a row to the result file
		WriteBytes( OutputStream, OutputRow );
	}

	OutputStream.close();
	if ( !OutputStream )
	{
		TerminateWithMessage( "Couldn't write the output file" );
	}
}

void BlurTGAFile( const std::string& InputFileName, const std::string& OutputFileName, float Sigma )
{
	// Open the input file
	std::ifstream InputStream( InputFileName, std::ios::binary );
	if ( !InputStream.is_open() )
	{
		TerminateWithMessage( "Couldn't open the input file" );
	}

	// Read Header and ImageId field
	const ImageHeaderTGA Header = ReadHeaderTGA( InputStream );
	CheckHeaderTGA( Header );
	const std::vector<uint8_t> ImageId = ReadBytes( InputStream, Header.IdLength, "Couldn't read ImageId field" );

	// Next field contains pixels
	const std::uint64_t PixelDataOffset = static_cast<std::uint64_t>( InputStream.tellg() );
	const std::uint64_t PixelDataSize = static_cast<std::uint64_t>( Header.Width ) * Header.Height * ( Header.PixelDepth / 8 );

	// The output file will have exactly the same size as the input one: same header, same amount of pixels, same tail
	std::error_code Error;  // For getting errors from std::filesystem
	const std::uintmax_t FileSize = std::filesystem::file_size( InputFileName, Error );
	if ( Error || FileSize < PixelDataOffset + PixelDataSize )
	{
		TerminateWithMessage( "File is too short to contain all pixels, specified in header" );
	}

	// Read all the pixels at once. The threads share them read-only, so each strip can take its neighbor rows from here
	// The size check above guarantees that the file really has this many bytes before we allocate memory for them inside the ReadBytes function
	const std::vector<uint8_t> Pixels = ReadBytes( InputStream, static_cast<std::size_t>( PixelDataSize ), "Couldn't read the pixels" );

	// Create the output file
	std::ofstream OutputStream( OutputFileName, std::ios::binary );
	if ( !OutputStream.is_open() )
	{
		TerminateWithMessage( "Couldn't create the output file" );
	}

	//  Write Header and ImageId field
	WriteHeaderTGA( OutputStream, Header );
	WriteBytes( OutputStream, ImageId );
	// Next field will contain pixels. The threads write them later, so skip it for now

	// Stand after the pixel data
	// InputStream already stands right after the pixels, as we have just read them
	OutputStream.seekp( static_cast<std::streamoff>( PixelDataOffset + PixelDataSize ) );

	// The check is needed because operator<< sets failbit when there is nothing to copy
	if ( InputStream.peek() != std::ifstream::traits_type::eof() )
	{
		// Copy everything after the pixels ( developer area, extension area, footer ) as is.
		OutputStream << InputStream.rdbuf();
	}

	OutputStream.close();
	if ( !OutputStream )
	{
		TerminateWithMessage( "Couldn't write the output file" );
	}

	// If there were no tail after pixel data then the file only contains Head and ImageId field,
	// so we need to resize it manually
	std::filesystem::resize_file( OutputFileName, FileSize, Error );
	if ( Error )
	{
		TerminateWithMessage( "Couldn't resize the output file" );
	}

	const int Radius = GetGaussianKernelRadiusFromSigma( Sigma );
	const std::vector<float> Kernel = MakeGaussianKernel( Radius, Sigma );  // Making Kernel out of Sigma and Radius

	// Split the image into horizontal strips, one per hardware thread.
	// hardware_concurrency() counts logical processors ( with Hyper-Threading ) and may return 0 if it can't tell
	const int ImageHeight = Header.Height;
	const int ThreadCount = std::clamp( static_cast<int>( std::thread::hardware_concurrency() ), 1, ImageHeight );
	const int RowsPerStrip = ( ImageHeight + ThreadCount - 1 ) / ThreadCount;  // Rounded up, so the strips cover all the rows

	{
		// jthread joins in its destructor, so leaving this scope waits until all the strips are written
		std::vector<std::jthread> Threads;
		Threads.reserve( ThreadCount );
		for ( int FirstRow = 0; FirstRow < ImageHeight; FirstRow += RowsPerStrip )
		{
			// Creating threads to calculate blur for strips from FirstRow to EndRow for each thread
			const int EndRow = std::min( FirstRow + RowsPerStrip, ImageHeight );
			Threads.emplace_back( [&, FirstRow, EndRow]() { BlurStripAndWriteToFile( Pixels, OutputFileName, Header, PixelDataOffset, FirstRow, EndRow, Kernel, Radius ); } );
		}
	}
}

}  // namespace Image
