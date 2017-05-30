function test(p)
{
//    print("starting test " +  test_cpt)
    for (var i = 0; i < N; i++) {
	for (var j = 0; j < N; j++) {
	    idx = i*N+j
	    p[idx] += 666666
	}
//        if ((i + 1) % (N / 5) == 0) {
//	    print("test " +  test_cpt + " is at i = " + i)
//	}
    }
//   test_cpt =  test_cpt + 1
}

var N = 10000
var test_cpt = 0
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
