function matmult(left, right, res,
		 left_nblines, left_nbcols, right_nbcols) {

    for (var i = 0; i < left_nblines; i++) {
	for (var j = 0; j < left_nbcols; j++) {
	    for (var k = 0; k < right_nbcols; k++) {
		var idx_res = i*right_nbcols + k;
		var idx_left = i*left_nbcols + j;
		var idx_right = j*right_nbcols + k;
		res[idx_res] = res[idx_res] +
		    left[idx_left] * right[idx_right];
	    }
	}
    }
}

function dump_matrix(m, nblines, nbcols) {
    for (var i = 0; i < nblines; i++) {
	var s = ""
	for (var j = 0; j < nbcols; j++) {
	    s = s + m[i*nbcols + j] + " "
	}
	print(s)
    }    
}

function fill_matrix(m, nblines, nbcols) {
    var cpt = 1;
    for (var i = 0; i < nblines; i++) {
	for (var j = 0; j < nbcols; j++) {
	    m[i*nbcols + j] = cpt;
	    cpt = cpt + 1;
	    if (cpt > 2) {
		cpt = 0;
	    }
	}
    }    
}

function fill_matrix_zero(m, nblines, nbcols) {
    for (var i = 0; i < nblines; i++) {
	for (var j = 0; j < nbcols; j++) {
	    m[i*nbcols + j] = 0
	}
    }    
}


// /////// Simple test ///////
// var left1_nblines = 3;
// var left1_nbcols = 3;
// var left1 = [1,2,3,4,5,6,7,8,9];

// var right1_nblines = 3;
// var right1_nbcols = 2;
// var right1 = [1,2,3,4,5,6];

// var res1 = [0,0,0,0,0,0];

// matmult(left1, right1, res1, left1_nblines, left1_nbcols, right1_nbcols)
// dump_matrix(res1, left1_nblines, right1_nbcols)

// /////// Small matrices with fill functions ///////
// var left2_nblines = 4;
// var left2_nbcols = 3;
// var left2 = Array();
// fill_matrix(left2, left2_nblines, left2_nbcols);
// dump_matrix(left2, left2_nblines, left2_nbcols)

// var right2_nblines = 3;
// var right2_nbcols = 2;
// var right2 = Array();
// fill_matrix(right2, right2_nblines, right2_nbcols);
// dump_matrix(right2, right2_nblines, right2_nbcols)

// var res2 = Array();
// fill_matrix_zero(res2, left2_nblines, right2_nbcols);

// print("Starting BIG matmul 1")
// matmult(left2, right2, res2, left2_nblines, left2_nbcols, right2_nbcols)
// dump_matrix(res2, left2_nblines, right2_nbcols)

//////// Big matrices ///////////
var left3_nblines = 6000;
var left3_nbcols = 3000;
var left3 = Array();
fill_matrix(left3, left3_nblines, left3_nbcols);

var right3_nblines = 3000;
var right3_nbcols = 30;
var right3 = Array();
fill_matrix(right3, right3_nblines, right3_nbcols);

var res3 = Array();
fill_matrix_zero(res3, left3_nblines, right3_nbcols);

print("Starting BIG matmul 1")
matmult(left3, right3, res3, left3_nblines, left3_nbcols, right3_nbcols)
print("Starting BIG matmul 2")
matmult(left3, right3, res3, left3_nblines, left3_nbcols, right3_nbcols)


