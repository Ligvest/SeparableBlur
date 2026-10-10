#include "ImageTGABlurer.h"

#include <chrono>
#include <iostream>

int main( int argc, char *argv[] )
{
	Image::ImageTGABlurer tga;

	const auto StartTime = std::chrono::steady_clock::now();
	tga.BlurAndSaveToFile( "Sharkmob.tga", "SharkmobBlur.tga", 30.0f );
	tga.PrintHeader();
	const auto EndTime = std::chrono::steady_clock::now();
	std::cout << "Blur took: " << std::chrono::duration<double, std::milli>( EndTime - StartTime ).count() << " ms\n";

	return 0;
}
