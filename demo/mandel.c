#include <stdio.h>
#include <time.h>
#include <math.h>
#include <malloc.h>

/*
{mandelbrot.bra

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

}

Mandelbrot=
  ( doit
  =
*/
/*
    .   1000:?iterations
      & 2000:?ypixels
      & (1|20/9):?ratio
      &   glf$('(<.$iterations))
        : (=?"<iterations")
      & 256*!iterations^-1:?colfactor
*/
#define iterations 1000.0
double ypixels = 2000.0;
#define colfactor 256.0 / iterations

static void* compiledCalcule(
    /*
      &
            ' (   (s.xpixels)
                  (s.ypixels)
                  (s.X)
                  (s.Y)
                  (s.R)
    */
    double xpixels,
    double ypixels,
    double X,
    double Y,
    double R
)
    {
    /*
              .   !X+-1*!R:?beginx
                & !X+!R:?endx
                & !Y+-1*!R:?beginy
                & !Y+!R:?endy
    */
    double beginx = X + -1.0 * R;
    double endx = X + R;
    double beginy = Y + -1.0 * R;
    double endy = Y + R;
    /*
                & (!endx+-1*!beginx)*(!xpixels+-1)^-1:?deltax
                & (!endy+-1*!beginy)*(!ypixels+-1)^-1:?deltay
    */
    double deltax = (endx + -1.0 * beginx) / (xpixels + -1.0);
    double deltay = (endy + -1.0 * beginy) / (ypixels + -1.0);
    /*
                & tbl$(T, !ypixels, !xpixels, 2)
    */
    void* ret = malloc(sizeof(double[(long)ypixels][(long)xpixels]));
    double(*T)[(long)xpixels] = ret;
    /*
                & -1+!ypixels:?i
    */
    double i = -1 + ypixels;
    /*
                &   whl
                  ' ( !i:~<0
    */
    while(i >= 0.0)
        {
        /*
                    & -1+!xpixels:?j
        */
        double j = -1 + xpixels;
        /*
                    &   whl
                      ' ( !j:~<0
        */
        while(j >= 0.0)
            {
            /*
                        & !j*!deltax+!beginx:?x0
                        & !i*!deltay+!beginy:?y0
                        & 0:?x:?y
                        & 1:?J
            */
            double x0 = j * deltax + beginx;
            double y0 = i * deltay + beginy;
            double x = 0.0;
            double y = 0.0;
            double J = 1;
            /*
                        &   whl
                          ' ( hypot$(!x,!y):~>2
            */
            while(hypot(x, y) <= 2.0)
                {
                /*
                            & !J:$"<iterations"
                */
                if(J >= iterations)
                    break;
                /*
                            & !x*!x+-1*!y*!y+!x0:?xtemp
                            & 2*!x*!y+!y0:?y
                            & !xtemp:?x
                */
                double xtemp = - y * y + x0 + x * x; /* Notice: C adds terms from left to right. Bracmat evaluates LHS, RHS and finally top. */
                y = 2.0 * x * y + y0;
                x = xtemp;
                /*
                            & 1+!J:?J
                            )
                */
                J = J + 1.0;
                }
            /*
                        &   (   !J:$"<iterations"
                              & floor$(!J*$colfactor)
                            | 0
                            )
                          : ?(idx$(T,!i,!j))
            */
            T[(long)i][(long)j] = J < iterations ? floor(J * colfactor) : 0.0;
            /*
                        & !j+-1:?j
                        )
            */
            j = j + -1.0;
            }
        /*
                    &!i + -1: ?i
                    )
        */
        i = i + -1.0;
        }
    /*
                )
            : ?calcule
          & new$(calculation,!calcule):?compiledCalcule
    */
    return ret;
    }

static void doit()
    {
    /*
          & ( ~
            |   lst$calc
              & out$"ITER DEFINED"
              & (compiledCalcule..print)$
              & out$"ITER PRINTED"
            )
          & ( flt
            =   e,d,m,s,f
              .     !arg:(?arg,~<0:?d)
                  & !arg:0
                |     ( -1*!arg:>0:?arg&-1
                      | 1
                      )
                    : ?s
                  &   10\L!arg
                    :   ?e
                      + ( 10\L?m
                        | 0&1:?m
                        )
                  & (   !m+1/2*1/10^!d:~<10
                      & 1+!e:?e
                      & !m*1/10:?m
                    |
                    )
                  & @( div$(!m+1/2*(1/10^!d:?d),!d)

                  : `%?f ?m
                     )
                  &   str
                    $ ( !s*!f
                        (!d:~1&"."|)
                        !m
                        E
                        !e
                      )
            )
          & (
              &   ( ~&"[0.2929859127507,0.6117848324958,1.0E-11]"
                  | &"[-0.0452407411,0.9868162204352258,2.7E-10]"
                  | ~&"[-0.7450,0.1102,0.005]"
                  | ~&"[-0.7463,0.1102,0.005]"
                  | ~&"[0.45272105023,0.396494224267,1.4E-10]"
                  | ~&"[-0.235125,0.827215,4.0E-5]"
                  | ~&"[-0.722,0.246,0.019]"
                  | "[-0.16070135,1.0375665,1.0E-7]"
                  )
                : ?jsn
              & get$(!jsn,MEM,JSN):(,#%?X #%?Y #%?R)
*/
    char json[] = "[-0.16070135,1.0375665,1.0E-7]";
    double X = -0.16070135;
    double Y = 1.0375665;
    double R = 1.0E-7;
    double xpixels = ypixels;
    /*
                |   -2:?beginx
                  & 47/100:?endx
                  & -112/100:?beginy
                  & 112/100:?endy
                  & 1/2*(!endx+-1*!beginx):?R
                  & 1/2*(!beginx+!endx):?X
                  & 1/2*(!beginy+!endy):?Y
                )
    */
    /*
              & clk$:?t0
    */
    time_t t0 = clock();
    /*
              & (       (compiledCalcule..calculate)
                      $ (!xpixels,!ypixels,!X,!Y,!R)
                    : ?Result
                  & out$(Result !Result)
                | out$"Calculation did not return a value."
                )
    */
    double(*T)[(long)xpixels] = compiledCalcule(xpixels, ypixels, X, Y, R);
    /*
              & out$(div$(clk$+-1*!t0,1) S)
    */
    time_t t1 = clock();
    printf("time: %d\n", (int)floor(((double)t1 - (double)t0) / (double)CLOCKS_PER_SEC));
    /*
              & (compiledCalcule..export)$(N,T):(,?eks)
              & out$calculated
              &   put
                $ (   str
                    $ ( P3\n
                        "#Mandelbrot"
                        \n
                        !xpixels
                        " "
                        !ypixels
                        \n
                        255
                        \n
                          map
                        $ ( (
                            =
                              .   !arg:(,?arg)
                                &   map
                                  $ ( (
                                      =
                                        .   !arg:
                                          | !arg " " !arg " " !arg \n
                                      )
                                    . !arg
                                    )
                            )
                          . !eks
                          )
                      )
                  , "MandelbrotSetArr.pgm"
                  , NEW WYD BIN
                  )
    */
    char name[256];
    sprintf(name, "MandelbrotSetArrC-%s.pgm", json);
    FILE* fp = fopen(name, "wb");
    fprintf(fp, "P3\n#Mandelbrot\n%d %d\n255\n", (int)xpixels, (int)ypixels);
    for(int k = 0; k < (long)xpixels; ++k)
        for(int m = 0; m < (long)ypixels; ++m)
            {
            int arg = (int)T[k][m];
            fprintf(fp, "%d %d %d\n", arg, arg, arg);
            }
    fclose(fp);
    time_t t2 = clock();
    printf("Total Time: %d\n", (int)floor(((double)t2 - (double)t0) / (double)CLOCKS_PER_SEC));
    /*
              & out$eksported
          )
    */
    }
/*
      ( new
      =
        .   ~
          |   clk$:?t0
            & (its.doit)$
            & clk$:?t1
            & out$(flt$(!t1+-1*!t0,3))
      );
*/
/*
    r=
      get'("mandelbrot.bra",TXT)
    & rmv$(str$(mandelbrot ".bak"))
    & ren$("mandelbrot.bra".str$(mandelbrot ".bak"))
    &   put
      $ ( "{mandelbrot.bra

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

    }

    "
        , "mandelbrot.bra"
        , NEW
        , BIN
        )
    & lst'(Mandelbrot,"mandelbrot.bra",APP)
    & put'(\n,"mandelbrot.bra",APP,BIN)
    & lst'(r,"mandelbrot.bra",APP)
    &   put
      $ (str$("\nnew'" Mandelbrot ";\n"),"mandelbrot.bra",APP,BIN)
    & ;

    new'Mandelbrot;
*/

int main()
    {
    doit();
    }