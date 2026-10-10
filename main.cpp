#include <chrono>
#include <fstream>
#include <iostream>

#include "TGABlur.h"
#include "TGAFormat.h"

int main( int argc, char* argv[] )
{
	std::ifstream InputStream( "Sharkmob.tga", std::ios::binary );
	Image::PrintHeaderTGA( Image::ReadHeaderTGA( InputStream ) );

	const auto StartTime = std::chrono::steady_clock::now();
	Image::BlurTGAFile( "Sharkmob.tga", "SharkmobBlur.tga", 30.0f );
	const auto EndTime = std::chrono::steady_clock::now();
	std::cout << "Blur took: " << std::chrono::duration<double, std::milli>( EndTime - StartTime ).count() << " ms\n";

	return 0;
}
