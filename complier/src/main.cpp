// #include <stdio.h>
// #include <stdlib.h>
// #include <stdbool.h>
// #include <float.h>
// #include <math.h> 

// double gradient = 0.1;
// double min = DBL_MAX;

// // EXAMPLE TOCHANGE
// static inline bool constraint(int* u_set, double v1, double v2) {
//     int sum = 0;
//     for (int i = 0; i < 5; i++) {
//         sum += u_set[i];
//     }
    
//     return sum <= 3 && 
//            2 * v1 - 1.2 * v2 >= 1 &&
//            fabs(u_set[1] * v1 + u_set[3] * v2 -2.2) < 1e-6;

// }


// // EXAMPLE TOCHANGE
// static inline double c_function(int u, double v1, double v2) {
//     int u_set[5];
//     for (int i = 0; i < 5; i++) {
//         u_set[i] = (u >> i) & 1;
//     }
//     if (!constraint(u_set, v1, v2)) return DBL_MAX;
//     double sum = 0;
//     for (int i = 0; i < 5; i++) {
//         sum += u_set[i] * u_set[i] - 3 * u_set[i];
//     }
//     return sum + 2 * (v1 * v1 + v2 * v2) - 4 * v1 * v2;
// }

// int main(int argc, char *argv[]) {
//     if (argc >= 2) {
//         gradient = atof(argv[1]);
//     }
//     for (int u = 0; u < 32; u++) {
//         for (double v1 = 1; v1 <= 4; v1 += gradient) {
//             for (double v2 = 0; v2 <= 3.5; v2 += gradient) {
//                 double tmp = c_function(u, v1, v2);
//                 min = min < tmp ? min : tmp; 
//             }
//         }
//     }
//     printf("%lf", min);
//     return 0;
// }