# Demonstration of floating point capability

Since 2023, Bracmat has a the ability to perform floating point calculations.
This folder contains program code in Java, C, Python and Bracmat to construct
a graphic of part of the Mandelbrot set. I have tried to use the exact same
algorithm in each language. This may not always be the optimal way to implement
a Mandelbrot set algorithm. A more `Pythonesque' solution would involve Numpy,
I guess. These comparisons are just meant to say that Bracmat's native floating
point handling is as fast as one may expect from an interpreted programming
language.

* 2000x2000 pixels
* X = -0.16070135;
* Y = 1.0375665;
* R = 1.0E-7;

| Language | source code         | computation | computation | computation | computation  | relative |
|          |                     |     (I)     |+ writing (I)|     (II)    |+ writing (II)|  (C=1)   |
| :--------| :-------------------|-----------: |-----------: |-----------: |------------: |--------: |
| C        | mandel.c            |          18 |          19 |          54 |           57 |        1 |
| Java     | Mandelbrot.java     |          40 |          41 |          91 |           92 | 1.7 -  2 |
| Bracmat  | mandelbrot.bra      |         139 |         144 |         485 |          495 |   8 -  9 |
| Python   | mandel.py           |         615 |         618 |        1651 |         1657 |  29 - 33 |

The measurements were done on a Dell precision 5530 (I) and a Toshiba Portégé Z30A (II).

