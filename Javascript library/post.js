		var globalNames = [
			"resultOK",
			"resultInvalidColourSpace",
			"resultInvalidBlockSize",
			"resultCantAllocateMemory",
			"resultTooBigImage",
			"key",
			"dataBuffer",
			"dataLength",
			"horizontalBlocksCount",
			"verticalBlocksCount",
			"coefficientsCount",
			"usableCoefficientsCount",
			"oneCoefficientsCount",
			"guaranteedCapacity",
			"maximumCapacity",
			"expectedCapacity",
			"extractableLength"
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
		var buffer = 0;

		var imageProperties = null;

		var expandBuffer = function (neededBufferLength) {
			if (neededBufferLength > bufferLength) {
				try {
					buffer = kernel.ccall("realloc", "number", ["number", "number"], [buffer, neededBufferLength]);
				} catch (exception) {
					throw cantAllocateMemoryError;
				}

				bufferLength = neededBufferLength;
			}
		};

		this.load = function (image, key) {
			if (imageProperties !== null) {
				kernel.ccall("destroy", null, [], []);

				imageProperties = null;
			}

			expandBuffer(Math.floor(image.length * 1.2)); // To avoid a realloc if the resulting image is bigger

			kernel.setValue(globalPointers.get("dataBuffer"), buffer, "i32");
			kernel.setValue(globalPointers.get("dataLength"), image.length, "i32");
			kernel.HEAPU8.set(image, buffer);

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

			var unpackArrayOfK = function (pointer) {
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
				"guaranteedCapacity": unpackArrayOfK(globalPointers.get("guaranteedCapacity")),
				"maximumCapacity": unpackArrayOfK(globalPointers.get("maximumCapacity")),
				"expectedCapacity": unpackArrayOfK(globalPointers.get("expectedCapacity")),
				"extractableLength": unpackArrayOfK(globalPointers.get("extractableLength"))
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

			buffer = kernel.getValue(globalPointers.get("dataBuffer"), "i32");
			bufferLength = kernel.getValue(globalPointers.get("dataLength"), "i32");

			return new Uint8Array(kernel.HEAPU8.subarray(buffer, buffer + bufferLength));
		};

		this.embed = function (data, k) {
			var neededBufferLength = Math.min(data.length, imageProperties.maximumCapacity.get(k));

			expandBuffer(neededBufferLength);

			kernel.setValue(globalPointers.get("dataBuffer"), buffer, "i32");
			kernel.setValue(globalPointers.get("dataLength"), neededBufferLength, "i32");
			kernel.HEAPU8.set(data.subarray(0, neededBufferLength), buffer);

			return kernel.ccall("embed", "number", ["number"], [k]);
		};

		this.reset = function () {
			kernel.ccall("reset", null, [], []);
		};

		this.extract = function () {
			var neededBufferLength = 0;

			for (var k = 1; k <= 7; ++k) {
				neededBufferLength += imageProperties.extractableLength.get(k);
			}

			expandBuffer(neededBufferLength);

			kernel.setValue(globalPointers.get("dataBuffer"), buffer, "i32");
			kernel.ccall("extract", null, [], []);

			var result = new Map();
			var data = buffer;

			for (var k = 1; k <= 7; ++k) {
				var dataLength = imageProperties.extractableLength.get(k);

				result.set(k, new Uint8Array(kernel.HEAPU8.subarray(data, data + dataLength)));
				data += dataLength;
			}

			return result;
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
				"containerProperties": properties,
				"k": k,
				"embeddedLength": embeddedLength,
				"image": raw.save()
			};
		});

		this.extract = expandMemory(function (image, key) {
			raw.load(image, key);

			return raw.extract();
		});
	};

	this.invalidColourSpaceError = invalidColourSpaceError;
	this.invalidBlockSizeError = invalidBlockSizeError;
	this.cantAllocateMemoryError = cantAllocateMemoryError;
	this.tooBigImageError = tooBigImageError;

	this.Raw = Raw;
	this.Simple = Simple;
}();
