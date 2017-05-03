/**
	\file

	The #Cover_container_read function reads DCT coefficients and other properties of an image. The coefficients can be examined and modified by the caller or by other library functions and then written into an image with #Cover_container_write. This function recreates the original image, but with new coefficient values.

	Only the first colour component of a JPEG image is used, other are copied unchanged.

	The header file contains an include guard.
*/

#ifndef COVER_CONTAINER_H
	/**
		Include guard.
	*/

	#define COVER_CONTAINER_H

	#include <stddef.h>
	#include <stdbool.h>
	#include <stdio.h>

	#include <jpeglib.h>

	/**
		Index of the used JPEG colour component.
	*/

	#define COVER_CONTAINER_COMPONENT_INDEX 0

	/**
		Count of DCT coefficients per block.
	*/

	#define COVER_CONTAINER_BLOCK_LENGTH 64

	/**
		Container structure. All its fields are read-only, but the #coefficients pointer can be used to modify the coefficients.
	*/

	struct Cover_container {
		/**
			LibJPEG decompressor context.
		*/

		struct jpeg_decompress_struct *decompressor;

		/**
			Horizontal blocks count.
		*/

		size_t width_in_blocks;

		/**
			Vertical blocks count.
		*/

		size_t height_in_blocks;

		/**
			Coefficients count.
		*/

		size_t coefficients_count;

		/**
			Array of coefficients of the first component.
		*/

		struct jvirt_barray_control *coefficients;

		/**
			Arrays of coefficients of all components.
		*/

		struct jvirt_barray_control **coefficients_arrays;
	};

	/**
		Initializes a #Cover_container structure.

		\param [out] context The structure to initialize.

		\param decompressor An initialized LibJPEG decompressor structure with a set image source. It must remain untouched by the caller for the lifetime of the context.

		\returns `true` on success or `false` if the image is incompatible.

		Needs no clean up.

		LibJPEG errors must be handled by the caller. It's safe to `longjmp` through the function.

		\see The header file description.
	*/

	bool Cover_container_read(struct Cover_container *context, struct jpeg_decompress_struct *decompressor);

	/**
		Creates a JPEG image from a #Cover_container structure.

		\param context The structure, initialized by #Cover_container_read.

		\param compressor An initialized LibJPEG compressor structure with a set image destination.

		LibJPEG errors must be handled by the caller. It's safe to `longjmp` through the function.

		\see The header file description.
	*/

	void Cover_container_write(struct Cover_container *context, struct jpeg_compress_struct *compressor);
#endif
