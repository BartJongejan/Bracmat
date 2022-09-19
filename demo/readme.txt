mandelbrot.bra
mandelbrot.c
mandelbrot.py

Demonstration of floating point capability with "potu". (Not vanilla Bracmat.)

Comparison with C and Python versions:

Potu has more overhead in iterating over all points. With a "calculation" that does nothing more than returning
"1000.0", Potu uses about 200 s. With the real Mandelbrot calculation, the used time is 400 s.

The C version uses a little over 10 s, so is 20-40 times faster than Potu.

The Python version, with a dummy version of the calculation that just returns 1000, uses under two seconds, but with the real Mandelbrot calculations it takes 500 s.

Conclusion: the code in Potu's "calculation" object is 2.5 times faster than Python code, but because Potu's native numbers are not that fast, iteration over all points is much slower in Potu than in Python. 