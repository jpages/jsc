function test(p)
{
    for (var i = 0; i < N; i++) {
	var val = p[i];
	val = val + 777777
	val = val * 41
	p[i] = val
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
