var Eph5 = new function () {
	var invalidColourSpaceError = new Error("Invalid colour space");
	var invalidBlockSizeError = new Error("Invalid block size");
	var cantAllocateMemoryError = new Error("Can't allocate memory");
	var tooBigImageError = new Error("Too big image");

	var Raw = function (memory) {
		var kernel = {};

		if (arguments.length >= 1) {
			kernel.TOTAL_MEMORY = memory;
		}
