/**
	\file

	Rang-Hash and Rang-JPEG algorithms, see the [specification](https://github.com/Kleshni/Cover/Rang specification).

	#Cover_Rang_initialize initializes a context, which can be used to extract the hidden data from a JPEG image with #Cover_Rang_extract or to embed with #Cover_Rang_embed. The embedding function doesn't change coefficients, it only plans the needed changes, #Cover_Rang_apply should be called to apply them.

	The context should be destroyed with #Cover_Rang_destroy to free allocated memory.

	The embedding algorithm needs two images with unnoticable differences: clear and modified. #Cover_Rang_modify_image can be used to derive a modified image from a clear.

	Low-level hashing functions are also provided. They can be used to apply Rang-Hash to other image and media formats or to implement Rang-JPEG with data blocks separation.

	The header file contains an include guard.
*/

#ifndef COVER_RANG_H
	/**
		Include guard.
	*/

	#define COVER_RANG_H

	#include <stddef.h>
	#include <stdint.h>
	#include <stdbool.h>

	#include <cover/container.h>
	#include <nettle/salsa20.h>

	/**
		Modifies an image with almost transparent Gaussian blur.

		\param width The image width in pixels.

		\param height The height.

		\param [out] data The image data to modify. Each pixel is represented as a 32-bit number with RGB in three least significant octets in any order. The most significant octet is alpha channel, its value is ignored and set to 255 in the output.

		\param blured_column An uninitialized array of `height` pixels for internal use.

		\param untouched_column Another uninitialized array of `height` pixels for internal use.

		Produces pixel-to-pixel equivalent result, as the following sequence in GIMP 2.8.18:
		- copy the layer;
		- apply Gaussian blur with 0,1 pixel radius on the top layer;
		- make the top layer 1,0 % opaque and merge it down.
	*/

	void Cover_Rang_modify_image(
		size_t width,
		size_t height,
		uint32_t *data,
		uint32_t *blured_column,
		uint32_t *untouched_column
	);

	/**
		Calculates a pseudorandom string for a bit index.

		\param length The string length.

		\param [out] hash The result will be xored to this buffer.

		\param context A Salsa20 context with a set key, but not a nonce.

		\param index The index. Must fit in `uint32_t`.

		\see The algorithm specification.
	*/

	static inline void Cover_Rang_xor_string(
		size_t length,
		uint8_t *hash,
		const struct salsa20_ctx *context,
		uint_fast32_t index
	) {
		uint8_t nonce[SALSA20_NONCE_SIZE] = {
			index & 0xff,
			index >> 8 & 0xff,
			index >> 16 & 0xff,
			index >> 24
		};

		struct salsa20_ctx current_context = *context;

		salsa20_set_nonce(&current_context, nonce);
		salsa20r12_crypt(&current_context, length, hash, hash);
	}

	/**
		Hashes a range of bits.

		\param length The hash length.

		\param [out] hash The result will be xored to this buffer.

		\param context A Salsa20 context with a set key, but not a nonce.

		\param start The index of the first bit of the range.

		\param count The length of the range. The last its index must fit in `uint32_t`.

		\param bits The bits to hash. Each byte carries 8 bits, ordered from the least significant to the most significant.

		\see The algorithm specification.
	*/

	void Cover_Rang_hash(
		size_t length,
		uint8_t *hash,
		const struct salsa20_ctx *context,
		uint_fast32_t start,
		size_t count,
		const uint8_t *bits
	);

	/**
		Finds the first set bit and calculates its pseudorandom string.

		\param length The string length.

		\param [out] hash The result will be xored to this buffer.

		\param context A Salsa20 context with a set key, but not a nonce.

		\param start The index of the first bit to check.

		\param bits The bits. Each byte carries 8 bits, ordered from the least significant to the most significant.

		The searched subarray must contain at least one set bit and it's index must fit in `uint32_t`.

		\see The algorithm specification.
	*/

	static inline uint_fast32_t Cover_Rang_hash_one(
		size_t length,
		uint8_t *hash,
		const struct salsa20_ctx *context,
		uint_fast32_t start,
		const uint8_t *bits
	) {
		uint_fast32_t i = start;

		while ((bits[i / 8] >> i % 8 & 1) != 1) {
			++i;
		}

		Cover_Rang_xor_string(length, hash, context, i);

		return i;
	}

	/**
		Tries to find a reverse hash.

		\param length The length of the hash.

		\param [out] padding_bits_count The count of available padding bits. The count of used padding bits will be stored here.

		\param [out] vector The current hash, xored with the desired hash and followed by the padding bits. The reverse hash will be stored here.

		\param full_padding If `true`, all available padding bits will be used.

		\param matrix An array of `8 * length + padding_bits_count` uninitialized arrays of `length` bytes each for internal use.

		\param context A Salsa20 context with a set key, but not a nonce.

		\param [out] indexes An array of different indexes of modifiable bits, their count must equal the number of the hash and padding bits.

		The function reorders the `indexes` array, so that each bit in the resulting `vector` corresponds to an item of the reordered array. Each byte of the vector carries 8 bits, ordered from the least significant to the most significant. If a bit in the resulting vector is set, the corresponding modifiable bit is to be changed.

		The indexes, that correspond to unused padding bits, will be left on their original positions.

		\returns `true` on success or `false` on a failure to find a non-singular matrix.

		\see The algorithm specification.
	*/

	bool Cover_Rang_unhash(
		size_t length,
		size_t *padding_bits_count,
		uint8_t *vector,
		bool full_padding,
		uint8_t **matrix,
		const struct salsa20_ctx *context,
		uint32_t *indexes
	);

	/**
		Entropy length.
	*/

	#define COVER_RANG_ENTROPY_LENGTH SALSA20_256_KEY_SIZE

	/**
		Recommended number of padding bits.
	*/

	#define COVER_RANG_DEFAULT_PADDING_BITS_COUNT 24

	/**
		Context structure. All its fields are read-only.
	*/

	struct Cover_Rang {
		/**
			Clear image structure. Used to save the result after embedding.
		*/

		struct Cover_container *clear;

		/**
			Modified image structure.
		*/

		struct Cover_container *modified;

		struct salsa20_ctx strings_PRNG;
		struct salsa20_ctx randomization_PRNG;

		/**
			Count of set least significant bits in the clear image.
		*/

		size_t set_count;

		/**
			Count of differences between the clear and modified images.
		*/

		size_t usable_count;

		size_t bit_array_length;

		uint8_t *payload;
		uint32_t *usable;
		uint8_t *direction;

		uint8_t *changes;
	};

	/**
		Initializes a context.

		\param [out] context The context to initialize.

		\param clear A clear image, initialized by #Cover_container_read. It must remain untouched by the caller for the lifetime of the context.

		\param modified A modififed image, initialized by #Cover_container_read, or `NULL`. If not `NULL`, it must have the same dimensions as the clear image and must remain untouched by the caller for the lifetime of the context.

		\param entropy #COVER_RANG_ENTROPY_LENGTH random bytes, or `NULL` if `modififed` is `NULL`, in this case the context is extraction-only.

		When embedding, clear coefficients are changed only by +1 or -1 even if the actual difference from the modified coefficient is greater.

		\returns `true` on success or `false` on a memory allocation failure.

		The context should be destroyed with #Cover_Rang_destroy.

		LibJPEG errors must be handled by the caller. It's safe to `longjmp` through the function, in this case #Cover_Rang_destroy must be called explicitly to free allocated memory.

		\see The header file description.
	*/

	bool Cover_Rang_initialize(
		struct Cover_Rang *context,
		struct Cover_container *clear,
		struct Cover_container *modified,
		const uint8_t *entropy
	);

	/**
		Destroys a context.

		\param context The context.
	*/

	void Cover_Rang_destroy(struct Cover_Rang *context);

	/**
		Applies the changes, planned by #Cover_Rang_embed.

		\param context An initialized context.

		Changes coefficients of the `context->clear` structure. #Cover_container_write can be used to create a JPEG image from it.

		After a call, the context should be destroyed with #Cover_Rang_destroy - other actions are undefined.

		\returns Count of changed coefficients.

		LibJPEG errors must be handled by the caller. It's safe to `longjmp` through the function.

		\see The header file description.
	*/

	size_t Cover_Rang_apply(struct Cover_Rang *context);

	/**
		Extracts data.

		\param context An initialized context.

		\param length The data length.

		\param [out] data The output data array.

		Ignores the changes, planned by #Cover_Rang_embed, extracts from the unchanged image.

		\see The header file description.
	*/

	void Cover_Rang_extract(struct Cover_Rang *context, size_t length, uint8_t *data);

	/**
		Tries to embed data.

		\param context An initialized context.

		\param length The length of the data.

		\param data The data.

		\param padding_bits_count The count of padding bits. #COVER_RANG_DEFAULT_PADDING_BITS_COUNT is usually sufficient.

		Can be called multiple times, successive calls reset previous changes until #Cover_Rang_apply is called. Saves previous changes in case of a failure.

		The caller must make sure, that the image capacity is sufficient: `context->usable_count` must be at least the number of data and padding bits.

		\returns 0 on success, 1 on a memory allocation failure, or 2 on a failure to find a non-singular matrix.

		\see The header file description.
	*/

	int Cover_Rang_embed(struct Cover_Rang *context, size_t length, const uint8_t *data, size_t padding_bits_count);
#endif
