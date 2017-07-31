// The Great Computer Language Shootout
// http://shootout.alioth.debian.org/
//
// modified by Isaac Gouy

var before = (new Date()).getTime(); // JSCPOLLY

function pad(number,width){
   var s = number.toString();
   var prefixWidth = width - s.length;
   if (prefixWidth>0){
      for (var i=1; i<=prefixWidth; i++) s = " " + s;
   }
   return s;
}

function nsieve(m, isPrime){
   var i, k, count;

   var before = (new Date()).getTime(); // JSCPOLLY
   for (i=2; i<=m; i++) { isPrime[i] = true; }
   count = 0;

   for (i=2; i<=m; i++){
      if (isPrime[i]) {
         for (k=i+i; k<=m; k+=i) isPrime[k] = false;
         count++;
      }
   }
   var diff = (new Date()).getTime() - before;print("access-nseive;" + diff); // JSCPOLLY
   return count;
}

function sieve() {
    var sum = 0;
    for (var i = 1; i <= 10; i++ ) {
        var m = (1<<i)*10000;
        var flags = Array(m+1);
        sum += nsieve(m, flags);
    }
    return sum;
}

var result = sieve();

var diff = (new Date()).getTime() - before;print("access-nseive;" + diff); // JSCPOLLY

var expected = 1430116;
if (result != expected)
    throw "ERROR: bad result: expected " + expected + " but got " + result;


