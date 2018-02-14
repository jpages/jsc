// res[i][j] = res[i][j] + left[i][k] * right[k][j];
// SLOW version
function matmultijk(left, right, res,
		    left_nblines, left_nbcols, right_nbcols) {

    // i j k 
    for (var i = 0; i < left_nblines; i++) {
	for (var j = 0; j < right_nbcols; j++) {
	    for (var k = 0; k < left_nbcols; k++) {
		var idx_res   = i * right_nbcols + j;
		var idx_left  = i * left_nbcols  + k;
		var idx_right = k * right_nbcols + j;
		res[idx_res] = res[idx_res] +
		    left[idx_left] * right[idx_right];
	    }
	}
    }
}

// res[i][j] = res[i][j] + left[i][k] * right[k][j];
// FAST version
function matmultikj(left, right, res,
		    left_nblines, left_nbcols, right_nbcols) {

    // i k j 
    for (var i = 0; i < left_nblines; i++) {
	for (var k = 0; k < left_nbcols; k++) {
	    for (var j = 0; j < right_nbcols; j++) {
		var idx_res   = i * right_nbcols + j;
		var idx_left  = i * left_nbcols  + k;
		var idx_right = k * right_nbcols + j;
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

//////// Big matrices ///////////
var left_nblines = 2000;
var left_nbcols = 3000;
var left = Array();
fill_matrix(left, left_nblines, left_nbcols);

var right_nblines = 3000;
var right_nbcols = 300;
var right = Array();
fill_matrix(right, right_nblines, right_nbcols);

var res = Array();
res_nblines = left_nblines
res_nbcols = right_nbcols 
fill_matrix_zero(res, res_nblines, res_nbcols);

var start_llint = new Date().getTime();
print("Starting BIG matmul ikj 1")
matmultikj(left, right, res, left_nblines, left_nbcols, right_nbcols)
var end_llint = new Date().getTime();
var time_llint = end_llint - start_llint;
print('Execution time ikj llint: ' + time_llint + ' milliseconds');

var start_ftl = new Date().getTime();
print("Starting BIG matmul ikj 2")
matmultikj(left, right, res, left_nblines, left_nbcols, right_nbcols)
var end_ftl = new Date().getTime();
var time_ftl = end_ftl - start_ftl;
print('Execution time ikj ftl BENCHMARK: ' + time_ftl + ' milliseconds');

//dump_matrix(res3, res3_nblines, res3_nbcols)

