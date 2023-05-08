class Mandelbrot {

static double iterations = 1000.0;
static double ypixels = 2000.0;
static double colfactor = 256.0 / iterations;


static void compiledCalcule(
    double xpixels,
    double ypixels,
    double X,
    double Y,
    double R,
    double[][] T
)
    {
    double beginx = X + -1.0 * R;
    double endx = X + R;
    double beginy = Y + -1.0 * R;
    double endy = Y + R;
    double deltax = (endx + -1.0 * beginx) / (xpixels + -1.0);
    double deltay = (endy + -1.0 * beginy) / (ypixels + -1.0);
    double i = -1 + ypixels;
    while(i >= 0.0)
        {
        double j = -1 + xpixels;
        while(j >= 0.0)
            {
            double x0 = j * deltax + beginx;
            double y0 = i * deltay + beginy;
            double x = 0.0;
            double y = 0.0;
            double J = 1;
            while(Math.hypot(x, y) <= 2.0)
                {
                if(J >= iterations)
                    break;
                double xtemp = -1.0 * y * y + x0 + x * x; /* Notice: C adds terms from left to right. Bracmat evaluates LHS, RHS and finally top. */
                y = 2.0 * x * y + y0;
                x = xtemp;
                J = J + 1.0;
                }
            T[(int)i][(int)j] = J < iterations ? Math.floor(J * colfactor) : 0.0;
            j = j + -1.0;
            }
        i = i + -1.0;
        }
    return;
    }

static void doit()
    {
    double X = -0.0452407411;
    double Y = 0.9868162204352258;
    double R = 2.7E-10;
    double xpixels = ypixels;
    double[][] T = new double[(int)ypixels][(int)xpixels];
    compiledCalcule(xpixels, ypixels, X, Y, R,T);
//    System.out.println("time: %d\n", (int)floor(((double)t1 - (double)t0) / (double)CLOCKS_PER_SEC));
    java.io.OutputStreamWriter writer = null;
    try 
        {
        writer = new java.io.OutputStreamWriter(new java.io.FileOutputStream("MandelbrotSetArrJava.pgm"));
        writer.write("P3\n#Mandelbrot\n"+String.valueOf((int)xpixels)+" "+String.valueOf((int)ypixels)+"\n255\n");
        for(int k = 0; k < (int)xpixels; ++k)
            for(int m = 0; m < (int)ypixels; ++m)
                {
                int arg = (int)T[k][m];
                String sarg = String.valueOf(arg);
                writer.write(sarg + " " + sarg + " " + sarg + "\n");
                }
        }
    catch (Exception e) 
        {
        e.printStackTrace();
        }
    finally 
        {
        try 
            {
            // Close the writer regardless of what happens...
            writer.close();
            }
        catch (Exception e) 
            {
            }
        }
//    time_t t2 = clock();
  //  printf("Total Time: %d\n", (int)floor(((double)t2 - (double)t0) / (double)CLOCKS_PER_SEC));
    }

public static void main(String[] args) 
    {
    long startTime = System.nanoTime();

    doit();
    long endTime = System.nanoTime();

    long duration = (endTime - startTime);
    System.out.println("Hello, World!" + String.valueOf(duration)); 
    }
}
