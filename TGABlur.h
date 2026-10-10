#pragma once

#include <string>

namespace Image
{

// Blurs a TGA image with a separable Gaussian and writes the result to OutputFileName.
// The image is processed in horizontal strips ( ranges of rows ) in parallel threads and is never loaded into memory as a whole.
void BlurTGAFile( const std::string& InputFileName, const std::string& OutputFileName, float Sigma );

}  // namespace Image
