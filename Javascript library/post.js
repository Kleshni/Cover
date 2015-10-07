		var globalNames = [
			"resultOK",
			"resultInvalidColourSpace",
			"resultInvalidBlockSize",
			"resultCantAllocateMemory",
			"resultTooBigImage",
			"keyLength",
			"key",
			"dataLength",
			"data",
			"horizontalBlocksCount",
			"verticalBlocksCount",
			"coefficientsCount",
			"usableCoefficientsCount",
			"oneCoefficientsCount",
			"guaranteedCapacity",
			"maximumCapacity",
			"expectedCapacity"
		];

		var globalPointersArray = kernel.ccall("export_globals", "number", [], []);
		var globalPointers = new Map();

		for (var i = 0; i < globalNames.length; ++i) {
			globalPointers.set(globalNames[i], kernel.getValue(globalPointersArray + i * 4, "i32"));
		}

		var errors = new Map([
			[globalPointers.get("resultInvalidColourSpace"), invalidColourSpaceError],
			[globalPointers.get("resultInvalidBlockSize"), invalidBlockSizeError],
			[globalPointers.get("resultCantAllocateMemory"), cantAllocateMemoryError],
			[globalPointers.get("resultTooBigImage"), tooBigImageError]
		]);

		var getError = function (pointer) {
			if (errors.has(pointer)) {
				return errors.get(pointer);
			} else {
				return new Error(kernel.Pointer_stringify(pointer));
			}
		};

		var bufferLength = 0;
		var buffer = undefined;

		var imageProperties = undefined;

		this.load = function (image, key) {
			if (imageProperties !== undefined) {
				kernel.ccall("destroy", null, [], []);

				imageProperties = undefined;
			}

			var neededBufferLength = Math.floor(image.length * 1.2); // To avoid a realloc if the resulting image is bigger

			if (neededBufferLength > bufferLength) {
				try {
					buffer = kernel.ccall("realloc", "number", ["number", "number"], [buffer || 0, neededBufferLength]);
				} catch (exception) {
					throw cantAllocateMemoryError;
				}

				bufferLength = neededBufferLength;
			}

			kernel.setValue(globalPointers.get("dataLength"), image.length, "i32");
			kernel.setValue(globalPointers.get("data"), buffer, "i32");
			kernel.HEAPU8.set(image, buffer);

			kernel.setValue(globalPointers.get("keyLength"), key.length, "i32");
			kernel.HEAPU8.set(key, globalPointers.get("key"));

			var result;

			try {
				result = kernel.ccall("load", "number", [], []);
			} catch (exception) {
				throw cantAllocateMemoryError;
			}

			if (result !== globalPointers.get("resultOK")) {
				throw getError(result);
			}

			var unpackCapacity = function (pointer) {
				var array = kernel.getValue(pointer, "i32");

				var result = new Map()

				for (var i = 0; i < 7; ++i) {
					result.set(i + 1, kernel.getValue(array + i * 4, "i32"));
				}

				return result;
			};

			imageProperties = {
				"horizontalBlocksCount": kernel.getValue(globalPointers.get("horizontalBlocksCount"), "i32"),
				"verticalBlocksCount": kernel.getValue(globalPointers.get("verticalBlocksCount"), "i32"),
				"coefficientsCount": kernel.getValue(globalPointers.get("coefficientsCount"), "i32"),
				"usableCoefficientsCount": kernel.getValue(globalPointers.get("usableCoefficientsCount"), "i32"),
				"oneCoefficientsCount": kernel.getValue(globalPointers.get("oneCoefficientsCount"), "i32"),
				"guaranteedCapacity": unpackCapacity(globalPointers.get("guaranteedCapacity")),
				"maximumCapacity": unpackCapacity(globalPointers.get("maximumCapacity")),
				"expectedCapacity": unpackCapacity(globalPointers.get("expectedCapacity"))
			};

			return imageProperties;
		};

		this.save = function () {
			kernel.setValue(globalPointers.get("dataLength"), bufferLength, "i32");

			var result;

			try {
				result = kernel.ccall("save", "number", [], []);
			} catch (exception) {
				throw cantAllocateMemoryError;
			}

			if (result !== globalPointers.get("resultOK")) {
				throw getError(result);
			}

			bufferLength = kernel.getValue(globalPointers.get("dataLength"), "i32");
			buffer = kernel.getValue(globalPointers.get("data"), "i32");

			return new Uint8Array(kernel.HEAPU8.subarray(buffer, buffer + bufferLength));
		};

		this.embed = function (data, k) {
			var neededBufferLength = Math.min(data.length, imageProperties.maximumCapacity.get(k));

			if (neededBufferLength > bufferLength) {
				try {
					buffer = kernel.ccall("realloc", "number", ["number", "number"], [buffer, neededBufferLength]);
				} catch (exception) {
					throw cantAllocateMemoryError;
				}

				bufferLength = neededBufferLength;
			}

			kernel.setValue(globalPointers.get("dataLength"), neededBufferLength, "i32");
			kernel.setValue(globalPointers.get("data"), buffer, "i32");
			kernel.HEAPU8.set(data.subarray(0, neededBufferLength), buffer);

			return kernel.ccall("embed", "number", ["number"], [k]);
		};

		this.reset = function () {
			kernel.ccall("reset", null, [], []);
		};

		this.extract = function (k) {
			var maximumLength = imageProperties.maximumCapacity.get(k);

			if (maximumLength > bufferLength) {
				try {
					buffer = kernel.ccall("realloc", "number", ["number", "number"], [buffer, maximumLength]);
				} catch (exception) {
					throw cantAllocateMemoryError;
				}

				bufferLength = maximumLength;
			}

			kernel.setValue(globalPointers.get("dataLength"), maximumLength, "i32");
			kernel.setValue(globalPointers.get("data"), buffer, "i32");

			var dataLength = kernel.ccall("extract", "number", ["number"], [k]);

			return new Uint8Array(kernel.HEAPU8.subarray(buffer, buffer + dataLength));
		};
	};

	var Simple = function (memory) {
		if (arguments.length < 1) {
			memory = 16 * 1024 * 1024;
		}

		var raw = new Raw(memory);

		var expandMemory = function (callback) {
			return function () {
				while (true) {
					try {
						return callback.apply(this, arguments);
					} catch (exception) {
						if (exception === cantAllocateMemoryError) {
							raw = new Raw(memory * 2);
							memory *= 2;
						} else {
							throw exception;
						}
					}
				}
			};
		};

		this.embed = expandMemory(function (data, image, key) {
			var properties = raw.load(image, key);

			var k = 7, embeddedLength = 0;

			for (; k > 1; --k) {
				if (properties.expectedCapacity.get(k) >= data.length) {
					break;
				}
			}

			while (true) {
				embeddedLength = raw.embed(data, k);

				if (k > 1 && embeddedLength < data.length) {
					--k;

					raw.reset();
				} else {
					break;
				}
			}

			return {
				"contsainerProperties": properties,
				"k": k,
				"embeddedLength": embeddedLength,
				"image": raw.save()
			};
		});

		this.extract = expandMemory(function (image, key) {
			raw.load(image, key);

			var result = new Map();

			for (var k = 1; k <= 7; ++k) {
				result.set(k, raw.extract(k));
			}

			return result;
		});
	};

	this.invalidColourSpaceError = invalidColourSpaceError;
	this.invalidBlockSizeError = invalidBlockSizeError;
	this.cantAllocateMemoryError = cantAllocateMemoryError;
	this.tooBigImageError = tooBigImageError;

	this.Raw = Raw;
	this.Simple = Simple;
}();
