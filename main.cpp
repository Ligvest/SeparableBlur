#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "TGABlur.h"
#include "TGAFormat.h"

// Turns the blur factor [0, 1] into Sigma in pixels.
// Factor 1 is calibrated on a random tga image ( 5184x3456 ), where Sigma 150 makes the image blurred beyond recognition.
// Sigma is scaled by the shorter side of the image, so factor 1 looks the same at any resolution.
static float BlurFactorToSigma( float BlurFactor, int ImageWidth, int ImageHeight )
{
	constexpr float ReferenceSigma = 150.0f;         // Sigma for factor 1 on the reference image
	constexpr float ReferenceShorterSide = 3456.0f;  // Shorter side of the reference image

	const float ShorterSide = static_cast<float>( std::min( ImageWidth, ImageHeight ) );
	const float MaxSigma = ReferenceSigma * ShorterSide / ReferenceShorterSide;

	// The power 1.5 gives finer control at small factors, where the eye notices the blur the most
	// Lesser effect to the lower values and greater effect to the higher values
	return std::pow( BlurFactor, 1.5f ) * MaxSigma;
}

int main( int argc, char* argv[] )
{
	// The blurring happens in the end of the function

	std::vector<std::string_view> Args( argv, argv + argc );

	// Hardcoded arguments for testing from the IDE. Uncomment to ignore the real command line
	// Args = { "Blur", "Sharkmob.tga", "SharkmobBlur.tga", "1.0" };

	if ( Args.size() != 4 )
	{
		std::cerr << "Usage: Blur <source-image> <destination-image> <blur-factor>\n"
		          << "Example: Blur Sharkmob.tga SharkmobBlur.tga 0.2\n";
		return 1;
	}

	// from_chars doesn't throw and doesn't depend on the locale. ParsedEnd check rejects trailing garbage like "0.2abc",
	// and the range check is written so that NaN fails it too
	float BlurFactor = 0.0f;
	const std::string_view FactorArg = Args[3];
	const auto [ParsedEnd, ParseError] = std::from_chars( FactorArg.data(), FactorArg.data() + FactorArg.size(), BlurFactor );
	if ( ParseError != std::errc() || ParsedEnd != FactorArg.data() + FactorArg.size() || !( BlurFactor >= 0.0f && BlurFactor <= 1.0f ) )
	{
		std::cerr << "Blur factor should be a number from 0.0 to 1.0\n";
		return 1;
	}

	const std::string InputFileName( Args[1] );
	const std::string OutputFileName( Args[2] );

	// The image size is needed to turn the blur factor into Sigma
	std::ifstream InputStream( InputFileName, std::ios::binary );
	if ( !InputStream.is_open() )
	{
		std::cerr << "Couldn't open the input file\n";
		return 1;
	}

	// Print Header in console
	const Image::ImageHeaderTGA Header = Image::ReadHeaderTGA( InputStream );
	Image::PrintHeaderTGA( Header );

	// Calculate Sigma for the Blur Factor from arguments
	const float Sigma = BlurFactorToSigma( BlurFactor, Header.Width, Header.Height );
	std::cout << "Blur factor " << BlurFactor << " -> Sigma " << Sigma << " px\n";

	// The blurring itself
	const auto StartTime = std::chrono::steady_clock::now();
	Image::BlurTGAFile( InputFileName, OutputFileName, Sigma );
	const auto EndTime = std::chrono::steady_clock::now();
	std::cout << "Blur took: " << std::chrono::duration<double, std::milli>( EndTime - StartTime ).count() << " ms\n";

	return 0;
}
