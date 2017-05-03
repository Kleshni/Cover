/**
	\file

	[DDT](https://github.com/desudesutalk/desudesutalk) version of the [F5](https://code.google.com/p/f5-steganography/) algorithm.

	#Cover_Eph5_initialize initializes a context, which can be used to extract the hidden data with #Cover_Eph5_extract or to embed with #Cover_Eph5_embed. The embedding function doesn't change coefficients, it only plans the needed changes, #Cover_Eph5_apply should be called to apply them.

	The context should be destroyed with #Cover_Eph5_destroy to free allocated memory.

	The header file contains an include guard.
*/

#ifndef COVER_EPH5_H
	/**
		Include guard.
	*/

	#define COVER_EPH5_H

	#include <stddef.h>
	#include <stdint.h>
	#include <stdbool.h>

	#include <cover/container.h>
	#include <nettle/arcfour.h>

	/**
		Maximum `k` value.
	*/

	#define COVER_EPH5_MAXIMUM_K 7

	/**
		Key length.
	*/

	#define COVER_EPH5_KEY_LENGTH ARCFOUR_MAX_KEY_SIZE

	/**
		Creates a key for #Cover_Eph5_initialize from a password.

		\param [out] key A buffer of #COVER_EPH5_KEY_LENGTH bytes to store the result.

		\param password The password.
	*/

	void Cover_Eph5_expand_password(uint8_t *key, const char *password);

	/**
		Context structure. All its fields are read-only.
	*/

	struct Cover_Eph5 {
		/**
			Corresponding image structure.
		*/

		struct Cover_container *image;

		/**
			Count of non-zero coefficients.
		*/

		size_t usable_count;

		/**
			Count of coefficients with the absolute value of 1.
		*/

		size_t one_count;

		/**
			Non-increasing array of minimum image capacities for different `k` values.

			Indexed by `k - 1`.
		*/

		size_t guaranteed_capacity[COVER_EPH5_MAXIMUM_K];

		/**
			Non-increasing array of maximum possible image capacities for different `k` values.

			Indexed by `k - 1`.
		*/

		size_t maximum_capacity[COVER_EPH5_MAXIMUM_K];

		/**
			Array of expected image capacities for different `k` values. The estimations are >= minimum and <= maximum capacities for the same `k`.

			Indexed by `k - 1`.
		*/

		size_t expected_capacity[COVER_EPH5_MAXIMUM_K];

		/**
			Equals the #maximum_capacity array.
		*/

		size_t extractable_length[COVER_EPH5_MAXIMUM_K];

		size_t bit_array_length;

		uint8_t *payload;
		uint8_t *usable;
		uint8_t *one;

		uint32_t *permutation;
		uint8_t *keystream;

		uint8_t *changes;
	};

	/**
		Initializes a context.

		\param [out] context The context to initialize.

		\param image A container structure, initialized by #Cover_container_read. It must remain untouched by the caller for the lifetime of the context.

		\param key A key, an array of #COVER_EPH5_KEY_LENGTH bytes. Can be generated from a password with #Cover_Eph5_expand_password.

		\param writable Indicates, if the context can be used for embedding.

		\returns `true` on success or `false` on a memory allocation failure.

		The context should be destroyed with #Cover_Eph5_destroy.

		LibJPEG errors must be handled by the caller. It's safe to `longjmp` through the function, in this case #Cover_Eph5_destroy must be called explicitly to free allocated memory.

		\see The header file description.
	*/

	bool Cover_Eph5_initialize(
		struct Cover_Eph5 *context,
		struct Cover_container *image,
		const uint8_t *key,
		bool writable
	);

	/**
		Destroys a context.

		\param context The context.
	*/

	void Cover_Eph5_destroy(struct Cover_Eph5 *context);

	/**
		Applies the changes, planned by #Cover_Eph5_embed.

		\param context An initialized context.

		\param [out] zeroed_count Saves count of zeroed coefficients here.

		Changes coefficients of the `context->image` structure. #Cover_container_write can be used to create a JPEG image from it.

		After a call, the context should be destroyed with #Cover_Eph5_destroy - other actions are undefined.

		\returns Count of changed coefficients, including zeroed.

		LibJPEG errors must be handled by the caller. It's safe to `longjmp` through the function.

		\see The header file description.
	*/

	size_t Cover_Eph5_apply(struct Cover_Eph5 *context, size_t *zeroed_count);

	/**
		Extracts data for all `k` values.

		\param context An initialized context.

		\param [out] data An array of #COVER_EPH5_MAXIMUM_K output arrays. Each array must have a corresponding length from the `context->extractable_length` list.

		Ignores the changes, planned by #Cover_Eph5_embed, extracts from the unchanged image.

		\see The header file description.
	*/

	void Cover_Eph5_extract(struct Cover_Eph5 *context, uint8_t **data);

	/**
		Embeds data.

		\param context An initialized context.

		\param length The length of the data.

		\param data The data.

		\param k The `k` value from 1 to #COVER_EPH5_MAXIMUM_K.

		Can be called multiple times, successive calls reset previous changes until #Cover_Eph5_apply is called.

		\returns Count of embedded bytes, can be less than the data length if the image capacity is insufficient.

		\see The header file description.
	*/

	size_t Cover_Eph5_embed(struct Cover_Eph5 *context, size_t length, const uint8_t *data, int k);
#endif
