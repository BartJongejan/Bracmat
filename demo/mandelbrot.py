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
    for J in range(1,1001):
        if math.hypot(x, y) >= 2.0 or J == 1000:
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
    for x0 in range(-2000, 471, 1):
        for y0 in range(-1120, 1121, 1):
            Niter = calculate(x0/1000.0, y0/1000.0)
            if Niter == 1000:
                alliters.append('  ')
            else:
                alliters.append('**')
        alliters.append('\n')
    print("looped")   
    textfile = open("mandelPy.txt", "w")
    a = textfile.write(''.join(alliters))
    textfile.close()
    t1 = time.perf_counter()
    print("done")
    print("Time %f\n", t1 - t0 )

doit()