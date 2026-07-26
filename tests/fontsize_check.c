/* Verify DPI/point-size font math: prints dpm, startup cell/points, and the
   cell and readback points after ami_setpoints() calls, to a plain file. */
#include <stdio.h>
#include <graphics.h>

int main(void)

{

    FILE* rf;

    rf = fopen("fontsize_results.txt", "w");
    if (!rf) return 1;
    fprintf(rf, "dpmx: %ld dpmy: %ld\n", ami_dpmx(stdout), ami_dpmy(stdout));
    fprintf(rf, "startup: cell %ldx%ld points %.2f\n",
            ami_chrsizx(stdout), ami_chrsizy(stdout), ami_points(stdout));
    ami_auto(stdout, 0); /* fixed font sizing requires auto off */
    ami_setpoints(stdout, 12.0f);
    fprintf(rf, "set 12.0pt: cell %ldx%ld points %.2f\n",
            ami_chrsizx(stdout), ami_chrsizy(stdout), ami_points(stdout));
    ami_setpoints(stdout, 10.5f);
    fprintf(rf, "set 10.5pt: cell %ldx%ld points %.2f\n",
            ami_chrsizx(stdout), ami_chrsizy(stdout), ami_points(stdout));
    ami_setpoints(stdout, 24.0f);
    fprintf(rf, "set 24.0pt: cell %ldx%ld points %.2f\n",
            ami_chrsizx(stdout), ami_chrsizy(stdout), ami_points(stdout));
    fclose(rf);
    ami_autohold(0); /* exit without holding the window */

    return 0;

}
