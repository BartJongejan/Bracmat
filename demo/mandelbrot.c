#include <math.h>
#include <stdio.h>
#include <time.h>

/*
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
*/

int calculate(double a0, double a1)
    {
    double x = 0.0;
    double y = 0.0;
    int J;
    for(J = 1;hypot(x, y) < 2.0 && J < 1000;++J)
        {
        double xtemp = x * x - y * y + a0;
        y = 2.0 * x * y + a1;
        x = xtemp;
        }
    return J;
    }

int main(int argc, char **argv)
    {
    double x0;
    static char alliters[100000000];
    char* p = alliters;
    FILE *fp;
    clock_t t0, t1;
    t0 = clock();
    for(x0 = -2.0; x0 <= 0.47; x0 += 0.001)
        {
        double y0;
        for(y0 = -1.12; y0 <= 1.12; y0 += 0.001)
            {
            int Niter = calculate(x0, y0);
            if(Niter == 1000)
                {
                *p++ = ' ';
                *p++ = ' ';
                }
            else
                {
                *p++ = '*';
                *p++ = '*';
                }
            }
        *p++ = '\n';
        }
    *p = 0;
    fp = fopen("mandelC.txt", "wb");
    fprintf(fp,"%s", alliters);
    fclose(fp);
    t1 = clock();
    printf("Time %f\n", (double)(t1 - t0) / (double)CLOCKS_PER_SEC);
    }