#pragma once

#include <cstddef>
#include <vector>

namespace Image
{

// Kernel radius for the given Sigma. Radius = Sigma * 3 - Good compromise between quality and performance
int GetGaussianKernelRadiusFromSigma( float Sigma );

// 1D Gaussian kernel of 2 * Radius + 1 weights. Weights are normalized to sum to 1, so the image doesn't get darker or brighter
std::vector<float> MakeGaussianKernel( int Radius, float Sigma );

// Blurs one row along X. Neighbors outside the image are taken from the edge pixel ( clamp )
// PaddedRow is a scratch buffer owned by the caller, so each thread can have its own one.
// PaddedRow will be overwritten inside the function.
void BlurRowHorizontally( const float* SrcRow, int ImageWidth, int ChannelsInPixel, const std::vector<float>& Kernel, std::vector<float>& PaddedRow, float* DstRow, int Radius );

// Blurs one row along Y: weighted sum of the same channel in 2 * Radius + 1 neighbor rows.
void BlurRowVertically( const float* const* SrcRows, std::size_t ChannelsInRow, const std::vector<float>& Kernel, float* DstRow );

}  // namespace Image
