function test(p)
{
    for (var i = 0; i < N; i++) {
	var val = p[i];
	val = val + 42;
	val = val * 2;
	val = val - 43;
	p[i] = val;
    }
}

var N = 10000000
var flat_pixels = [];
for (var i = 0; i < N; i++) {
    flat_pixels[i] = i
}

test(flat_pixels);
print(flat_pixels[777])
test(flat_pixels);
print(flat_pixels[777])
test(flat_pixels);
print(flat_pixels[777])
test(flat_pixels);
print(flat_pixels[777])
