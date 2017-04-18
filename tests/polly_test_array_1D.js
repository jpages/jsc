function test(flat_pixels)
{
    for (var i = 0; i < N; i++) {
	for (var j = 0; j < N; j++) {
	    flat_pixels[i*N+j] += 666666
	}
    }
}

var N = 3000
var flat_pixels = [];
for (var i = 0; i < N; i++) {
    for (var j = 0; j < N; j++) {
	flat_pixels[i*N+j] = i + j
    }
}
print(flat_pixels[50*N+50])
test(flat_pixels);
print(flat_pixels[50*N+50])
test(flat_pixels);
print(flat_pixels[50*N+50])
