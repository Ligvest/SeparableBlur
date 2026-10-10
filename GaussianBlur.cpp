#include "GaussianBlur.h"

#include <algorithm>
#include <cmath>

namespace Image
{

int GetGaussianKernelRadiusFromSigma( float Sigma )
{
	return static_cast<int>( std::ceil( 3.0f * Sigma ) );
}

std::vector<float> MakeGaussianKernel( int Radius, float Sigma )
{
	// No Blur
	if ( Sigma <= 0.0f )
	{
		return { 1.0f };
	}

	// 1D Kernel. Radius of Pixels before the center + the same amount after the center and + 1 pixel in the center
	std::vector<float> Kernel( static_cast<std::size_t>( 2 * Radius + 1 ) );

	// A sum to normalize kernel later
	float Sum = 0.0f;
	for ( int Offset = -Radius; Offset <= Radius; ++Offset )
	{
		// Using formula for calculating Separable Gaussian Blur weights
		const float Weight = std::exp( -static_cast<float>( Offset * Offset ) / ( 2.0f * Sigma * Sigma ) );

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

void BlurRowHorizontally( const float* SrcRow, int ImageWidth, int ChannelsInPixel, const std::vector<float>& Kernel, std::vector<float>& PaddedRow, float* DstRow, int Radius )
{
	// Pad the row with Radius copies of the edge pixel on each side ( "clamp to edge" ), so the blur loop below needs no bounds checks
	PaddedRow.resize( static_cast<std::size_t>( ImageWidth + 2 * Radius ) * ChannelsInPixel );
	// From the (left image edge - Radius) pos to the (right image edge + Radius ) pos
	for ( int X = -Radius; X < ImageWidth + Radius; ++X )
	{
		// ClampedX is just clamped position to return the last pixel if we go over the edge of the image
		// We clamp it because our loop goes beyond the edges of the image
		const int ClampedX = std::min( std::max( X, 0 ), ImageWidth - 1 );

		for ( int CurChannel = 0; CurChannel < ChannelsInPixel; ++CurChannel )
		{
			const int ClampedRowChannelPos = ClampedX * ChannelsInPixel + CurChannel;
			// (X + Radius) - because indices for PaddedRow are from 0 to ImageWidth + (2 * Radius)
			const int PaddedRowChannelPos = ( X + Radius ) * ChannelsInPixel + CurChannel;
			PaddedRow[PaddedRowChannelPos] = SrcRow[ClampedRowChannelPos];
		}
	}

	// Blurring
	const std::size_t ChannelsInRow = static_cast<std::size_t>( ImageWidth ) * ChannelsInPixel;
	// We accumulate the result in DstRow, so we need to 0 it first
	std::fill( DstRow, DstRow + ChannelsInRow, 0.0f );
	for ( int KernelIndex = 0; KernelIndex <= 2 * Radius; ++KernelIndex )
	{
		const float Weight = Kernel[KernelIndex];
		// Kernel and PaddedRow have the same shift, so element I of ShiftedRow is the neighbor at offset ( KernelIndex - Radius ) of element I of DstRow
		const float* ShiftedRow = PaddedRow.data() + static_cast<std::size_t>( KernelIndex ) * ChannelsInPixel;
		for ( std::size_t I = 0; I < ChannelsInRow; ++I )
		{
			// Here we are filling each DstRow channel with each ShiftedRow channel with a KernelIndex shift.
			// Then we change the shift and repeat the procedure. Filling the whole DstRow at a time little by little to be able to
			// apply SIMD, instead of blurring the whole pixel each time
			DstRow[I] += Weight * ShiftedRow[I];
		}
	}
}

void BlurRowVertically( const float* const* SrcRows, std::size_t ChannelsInRow, const std::vector<float>& Kernel, float* DstRow )
{
	// We accumulate the result in DstRow, so we need to 0 it first
	std::fill( DstRow, DstRow + ChannelsInRow, 0.0f );

	for ( std::size_t KernelIndex = 0; KernelIndex < Kernel.size(); ++KernelIndex )
	{
		const float Weight = Kernel[KernelIndex];
		const float* SrcRow = SrcRows[KernelIndex];
		// Go through all the channels in a row and add weighted channels to the resulting row
		for ( std::size_t I = 0; I < ChannelsInRow; ++I )
		{
			DstRow[I] += Weight * SrcRow[I];
		}
	}
}

}  // namespace Image
