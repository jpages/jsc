function test(pixels)
{
    for (var y = 0; y < 5000; y++) {
        for (var x = 0; x < 5000; x++) {
            pixels[y][x] += 666666;
        }
    }
    print (pixels[145][478])
}

var pixels = new Array();
for (var y = 0; y < 5000; y++) {
    pixels[y] = new Array();
    for (var x = 0; x < 5000; x++) {
        pixels[y][x] = 0;
    }
}
print (pixels[145][478])
test(pixels);
