function test2(flat_pixels)
{
    for (var i = 0; i < 5000; i++) {
	for (var j = 0; j < 5000; j++) {
	    flat_pixels[i*5000+j] = 666666
	}
    }
}

function main()
{
    var flat_pixels = [];
    for (var i = 0; i < 5000; i++) {
	pixels[i] = [];
	for (var j = 0; j < 5000; j++) {
            pixels[i][j] = 0;
	    flat_pixels[i*5000+j] = 0
	}
    }
    test2 (flat_pixels);
}

main()
