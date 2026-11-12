# TGA Blur

A command-line tool that blurs uncompressed 24- and 32-bit TGA images with a separable Gaussian blur.
Written in C++20 with only the STL, multithreaded, with vectorization-friendly (SIMD) inner loops.

```
Blur <source-image> <destination-image> <blur-factor>
Blur Photo.tga PhotoBlurred.tga 0.2
```

`blur-factor` is a number from `0.0` (no blur) to `1.0` (blurred beyond recognition).

I originally wrote it as a test assignment and kept working on it as a personal project to dig into the TGA format, blur algorithms and CPU performance.


## Features

- **TGA 2.0 support**: uncompressed true-color images, 24-bit (BGR) and 32-bit (BGRA). The ImageId field and the whole tail after the pixel data (developer area, extension area, footer) are copied to the output unchanged.
- **Separable Gaussian blur**: two 1D passes instead of one 2D pass, so the cost per pixel is O(r) instead of O(r²) with exactly the same result.
- **Resolution-independent blur factor**: the same blur-factor gives the same look on a 512x512 image and on an 8K image.
- **Correct alpha handling**: 32-bit images are blurred with premultiplied alpha, so transparent pixels don't create dark halos around visible ones.
- **Multithreading**: the image is split into horizontal strips, one per hardware thread (`std::jthread`), with no locks and no duplicated work.
- **Vectorization**: the hot loops are written so the compiler auto-vectorizes them (verified with `/Qvec-report:2`).
- **Predictable memory**: RAM used is about the size of the image, independent of the blur strength and the number of threads.


## Building

- C++20 is required (`std::jthread`).
- Tested with MSVC 19.44 (Visual Studio 2026, toolset v143) on x64. Open `Blur.sln` and build **Release | x64**.
- The measurements below use `/O2 /arch:AVX2`. Without AVX2 the loops are still vectorized with SSE2, but slower.


## How it works

```
* I commented almost every line of code to explain the algorithms, so for more detailed description look there. *
	Parse program arguments
	Make sure Blur Factor is between 0 and 1
	Convert Blur Factor to Sigma and Radius for Separable Gaussian Blur
	Run Blur Procedure:
		Read TGA Header to get info about the image
		Read ImageId Field to write it to out file
		Read all Pixel data to a std::vector
		Premultiply alpha-channels in PixelData to not let transparent colors affect the output image
		Write to out file Header, ImageId and a tail ( the data after the pixel data field )
		Calculate GaussianKernel
		Calculate amount of threads to use
		Divide the pixel data between threads ( each thread will get its range of rows to blur )
		Run Threads. Each one will blur its own strip ( range of rows ):
			Collect neighbor rows for vertical blur of range [CurRow - KernelRadius ... CurRow + KernelRadius]
			Do BlurRowVertically:
				Here we just weight each channel of all neighbor rows and add them to the according channel of destination row ( blur the destination row )
			Do BlurRowHorizontally:
				Make a "padded row". This is the row of size of image row width + (kernel radius to the left + kernel radius to the right).
				It's done to process a case when we are blurring edge pixels and they have no neighbors outside of the image. 
				In this case all the "outside" pixel just repeat the edge pixel of the image ("clamp to the edge").
				PaddedRow is built from vertical blur result so its pixels already "vertically blurred".
				Now for each channel of destination row we weight and add according channel of padded row, shifted by KernelIndex ( KernelIndex is an offset from -Radius to +Radius).
				Do this for each channel and each kernel index and we are done. Now the row is blurred wholly
			"Unpremultiply" alpha-channels as an opposite operation to Premultiply alpha to return back to normal colors
			We did all the blur, using float type so before writing the result in the file we convert the result row from float to uint8_t.
			Write the result into the file
			Repeat for each row in the thread's strip
		jthreads will wait for join in their destructor, so after all the threads finish the blurred result image should be ready.
```

The source pixels are shared between the threads read-only, so they need no synchronization. Every thread writes its own range of the output file through its own stream.


## Design decisions

**Blur factor -> Sigma.** `Sigma = 150 * ShorterSide / 3456 * BlurFactor^1.5`.
Factor 1 was calibrated on a 5184x3456 photo, where Sigma 150 makes it unrecognizable (by my own judgment), and it scales with the shorter side of the image. The power 1.5 gives finer control at small factors, where the eye notices blur the most.

**Kernel radius = ceil( 3 * Sigma ).** The Gaussian never reaches zero, so the kernel has to be cut. Beyond 3 Sigma all the weights together make 0.27% of the kernel, which is less than one level of an 8-bit channel ( 1/255 ≈ 0.39% ), so a bigger radius would only cost time.

**Vertical pass first, then horizontal.** The result is the same in any order, but with many threads the order matters: each strip needs R extra rows above and below it. If the horizontal pass goes first, those extra rows have to be blurred by several threads. If the vertical pass goes first, they are only read. This change alone made the multithreaded version about 2x faster on strong blur.

**Whole image in memory instead of streaming.** The first version streamed the file row by row with a per-thread ring buffer of 2R+1 float rows. That looked memory-friendly, but for strong blur on many threads the ring buffers grew far beyond the image itself ( estimated ~2 GB for an 8K image on 16 threads ). Reading the pixel data once and sharing it between the threads keeps the memory at about the size of the image and also made the program ~2x faster.

**Vectorization-friendly loops.** Instead of computing the full weighted sum for one pixel at a time, every pass adds one kernel weight to the whole row. The iterations are independent and the memory is contiguous, so the compiler processes 8 floats per AVX2 instruction.

**Premultiplied alpha.** Before the blur the colors are multiplied by the opacity, after the blur they are divided back. Otherwise the invisible color of fully transparent pixels ( usually black ) leaks into the visible neighbors.

**Rounding.** All float -> integer conversions round to the nearest value (I add 0.5 before conversion) instead of truncating, so the image doesn't get darker by half a level on average.


## Performance

Release, `/O2 /arch:AVX2`, measured on `Intel Core i9-12900H CPU (14 cores, 20 threads)`.

| Image | Blur | Version | Time |
|---|---|---|---|
| 5184x3456, 24-bit | Sigma 30 | single thread, scalar loops | ~5.0 s |
| 5184x3456, 24-bit | Sigma 30 | single thread, vectorized loops | ~2.0 s |
| 5184x3456, 24-bit | Sigma 200 | multithreaded, horizontal pass first | ~10.0 s |
| 5184x3456, 24-bit | Sigma 200 | multithreaded, vertical pass first | ~4.6 s |
| 7680x4320, 32-bit | factor 1 ( Sigma ~187 ) | final version | ~5 s, ~130 MB RAM |


## Project structure

| File | Responsibility |
|---|---|
| `main.cpp` | Argument parsing, blur factor -> Sigma, timing |
| `TGAFormat.h/.cpp` | TGA header: read, write, validate, print |
| `GaussianBlur.h/.cpp` | Kernel and the two 1D passes. Knows nothing about files or TGA |
| `TGABlur.h/.cpp` | The whole pipeline: reading, alpha, threads, writing |
| `BinaryIO.h/.cpp` | Reading and writing raw bytes with error checks |
| `Error.h` | Fatal error reporting |


## Limitations and next steps

- **Strong blur cost.** The Gaussian costs O(radius) per pixel. For very strong blur-factor, where smoothness is not so important, the next step would be an algorithm whose cost doesn't depend on the radius, such as a multi-pass box blur with running sums.
- **SIMD depends on the compiler.** The loops rely on auto-vectorization and on the `/arch:AVX2` flag. Explicit SIMD intrinsics would make the speed independent of the build settings.
- **Blur is tied to the TGA pipeline.** The 1D passes are format-independent, but the orchestration reads and writes TGA directly. A separate `Image` type ( width, height, channels, pixels ) with format-specific loaders and writers would make the blur reusable for other formats.
- **Supported formats.** Only uncompressed true-color TGA. RLE-compressed, color-mapped and grayscale images are rejected with an error message.
- **Error handling.** Errors print a message and call `std::terminate`, also inside worker threads. A more graceful approach would pass errors up to `main()`, delete the incomplete output file and return a non-zero exit code.
- **Unused alpha.** Some old writers store 32-bit TGA with an unused alpha byte equal to 0 (alpha == 0 usually means transparent pixel). The TGA 2.0 "Attributes Type" field, which tells whether the alpha channel is actually used, isn't read in my program, so such an image would be treated as fully transparent.