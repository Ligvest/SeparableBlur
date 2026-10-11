#pragma once

#include <string>

namespace Image
{

// Blurs a TGA image with a separable Gaussian and writes the result to OutputFileName.
// The pixel data is read into memory once and shared read-only by parallel threads, each blurring its own horizontal strip ( range of rows ).
void BlurTGAFile( const std::string& InputFileName, const std::string& OutputFileName, float Sigma );

}  // namespace Image
