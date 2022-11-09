''''
for each pixel (Px, Py) on the screen do
    x0 := scaled x coordinate of pixel (scaled to lie in the Mandelbrot X scale (-2.00, 0.47))
    y0 := scaled y coordinate of pixel (scaled to lie in the Mandelbrot Y scale (-1.12, 1.12))
    x := 0.0
    y := 0.0
    iteration := 0
    max_iteration := 1000
    while (x*x + y*y ≤ 2*2 AND iteration < max_iteration) do
        xtemp := x*x - y*y + x0
        y := 2*x*y + y0
        x := xtemp
        iteration := iteration + 1

    color := palette[iteration]
    plot(Px, Py, color)
'''

import time
import math 

def calculate(a0, a1):
    x = 0.0;
    y = 0.0;
    for J in range(1,5001):
        if math.hypot(x, y) >= 2.0 or J == 5000:
            break
        xtemp = x * x - y * y + a0
        y = 2.0 * x * y + a1
        x = xtemp
    return J
'''
 504.7275418999998 s
'''

'''
def calculate(a0, a1):
    return 1000
 1.390195500000118 s
'''

def doit():
    alliters = []
    t0 = time.perf_counter()
    X = -0.0452407411
    Y = 0.9868162204352258
    R = 2.7E-10
    x0 = X - 2 * R
    endx = x0 + 4 * R
    beginy = Y - R
    endy = beginy + 2 * R
    delta = R / 500.0

    while x0 <= endx:
        y0 = beginy
        while y0 <= endy:
            Niter = calculate(x0, y0)
            if Niter == 5000:
                alliters.append(' ')
            else:
                alliters.append('*')
            y0 = y0 + delta
        alliters.append('\n')
        x0 = x0 + 2*delta
    print("looped")   
    textfile = open("mandelPy.txt", "w")
    a = textfile.write(''.join(alliters))
    textfile.close()
    t1 = time.perf_counter()
    print("done")
    print("Time %f\n", t1 - t0 )

doit()