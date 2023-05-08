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

iterations = 1000.0
xpixels = ypixels = 2000.0
colfactor = 256.0 / iterations


def compiledCalcule(xpixels,ypixels,X,Y,R):
    beginx = X + -1.0 * R
    endx = X + R
    beginy = Y + -1.0 * R
    endy = Y + R
    deltax = (endx + -1.0 * beginx) / (xpixels + -1.0)
    deltay = (endy + -1.0 * beginy) / (ypixels + -1.0)
    i = -1 + ypixels
    T = [[0 for x in range(math.floor(xpixels))] for y in range(math.floor(ypixels))] 
    while i >= 0.0:
        j = -1 + xpixels
        while j >= 0.0:
            x0 = j * deltax + beginx
            y0 = i * deltay + beginy
            x = 0.0
            y = 0.0
            J = 1.0
            while math.hypot(x, y) <= 2.0:
                if J >= iterations:
                    break
                xtemp = -1.0 * y * y + x0 + x * x
                y = 2.0 * x * y + y0
                x = xtemp
                J = J + 1.0
            if J < iterations:
                T[math.floor(i)][math.floor(j)] = math.floor(J * colfactor)
            else:
                T[math.floor(i)][math.floor(j)] = 0.0

            j = j + -1.0
        i = i + -1.0
    return T
'''
 504.7275418999998 s
'''

'''
def calculate(a0, a1):
    return 1000
 1.390195500000118 s
'''

def doit():
    X = -0.0452407411
    Y = 0.9868162204352258
    R = 2.7E-10

    t0 = time.perf_counter()

    T = compiledCalcule(xpixels,ypixels,X,Y,R)

    t1 = time.perf_counter()
    print("Time:", t1 - t0 )

    print("\nlooped")   
    textfile = open("mandelPy.pgm", "w")

    textfile.write("P3\n#Mandelbrot\n" + str(xpixels) + " " + str(math.floor(ypixels)) + "\n255\n")
    for k in range(0,math.floor(xpixels)):
        for m in range(0,math.floor(ypixels)):
            arg = math.floor(T[k][m])
            textfile.write( str(arg) + " " + str(arg) + " " + str(arg) + "\n")
    textfile.close()

    print("done")
    t2 = time.perf_counter()
    print("Total Time:", t2 - t0 )

doit()