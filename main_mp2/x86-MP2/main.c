#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

extern void saxpy_asm(float* z, const float* x, size_t n, const float* y, float a);

void saxpy_c(float* z, const float* x, const float* y, float a, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        z[i] = a * x[i] + y[i];
    }
}

void* aligned_malloc(size_t bytes, size_t align) {
    return _aligned_malloc(bytes, align);
}
void aligned_free(void* p) {
    _aligned_free(p);
}

void initialize_vectors(float* x, float* y, size_t n) {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < n; i++) {
        x[i] = (float)(rand() % 100) / 10.0f;
        y[i] = (float)(rand() % 100) / 10.0f;
    }
}

double time_kernel_repeat_c(float* z, const float* x, const float* y, float a, size_t n, int repeats) {
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);

    // warm-up
    saxpy_c(z, x, y, a, n);

    QueryPerformanceCounter(&t0);
    for (int r = 0; r < repeats; ++r) {
        saxpy_c(z, x, y, a, n);
    }
    QueryPerformanceCounter(&t1);
    double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
    return elapsed / (double)repeats;
}

double time_kernel_repeat_asm(float* z, const float* x, const float* y, float a, size_t n, int repeats) {
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);

    // warm-up
    saxpy_asm(z, x, n, y, a);

    QueryPerformanceCounter(&t0);
    for (int r = 0; r < repeats; ++r) {
        saxpy_asm(z, x, n, y, a);
    }
    QueryPerformanceCounter(&t1);
    double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
    return elapsed / (double)repeats;
}

int vectors_equal(const float* a, const float* b, size_t n, float tol) {
    for (size_t i = 0; i < n; ++i) {
        float d = fabsf(a[i] - b[i]);
        if (d > tol) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char** argv) {
    const int repeats = 30;
    const int exps[] = { 20, 24, 28 }; // 2^20, 2^24, 2^28
    const int exps_count = sizeof(exps) / sizeof(exps[0]);
    const float A = 2.0f;

    int max_override = -1;
    if (argc >= 2) max_override = atoi(argv[1]);

    printf("==================================================\n");
    printf("   SAXPY Performance Comparison (Z = A*X + Y)  \n");
    printf("==================================================\n");
    printf("Scalar A = %.1f\n", A);
    printf("Iterations per test: %d\n\n", repeats);

    for (int i = 0; i < exps_count; ++i) {
        int p = exps[i];
        if (max_override >= 0 && p > max_override) {
            printf("Skipping 2^%d due to override %d\n", p, max_override);
            continue;
        }
        size_t n = (size_t)1 << p;
        printf("\n--------------- n = 2^%d = %zu ---------------\n", p, n);

        unsigned long long bytes_req = (unsigned long long)n * sizeof(float) * 3ULL;
        double giB = (double)bytes_req / (1024.0 * 1024.0 * 1024.0);
        printf("Approx memory for X,Y,Z arrays: %.3f GiB\n", giB);
        printf("Scalar A: %.1f\n", A);

        printf("--------------------------------------------------\n");

        // allocating aligned memory (16-byte alignment)
        float* x = (float*)aligned_malloc(n * sizeof(float), 16);
        float* y = (float*)aligned_malloc(n * sizeof(float), 16);
        float* z_c = (float*)aligned_malloc(n * sizeof(float), 16);
        float* z_asm = (float*)aligned_malloc(n * sizeof(float), 16);

        if (!x || !y || !z_c || !z_asm) {
            fprintf(stderr, "Allocation failed for n=%zu (need ~%.3f GiB). Try lowering exponent or free memory.\n", n, giB);
            aligned_free(x); aligned_free(y); aligned_free(z_c); aligned_free(z_asm);
            continue;
        }

        initialize_vectors(x, y, n);
        memset(z_c, 0, n * sizeof(float));
        memset(z_asm, 0, n * sizeof(float));

        printf("\nFirst 10 elements of X (randomized):\nX = [");
        for (int i = 0; i < 10; i++) {
            printf("%.2f", x[i]);
            if (i < 9) printf(", ");
        }
        printf("]\n");

        printf("First 10 elements of Y (randomized):\nY = [");
        for (int i = 0; i < 10; i++) {
            printf("%.2f", y[i]);
            if (i < 9) printf(", ");
        }
        printf("]\n");

        printf("\nBeginning performance evaluation...\n");

        double t_c = time_kernel_repeat_c(z_c, x, y, A, n, repeats);
        double t_asm = time_kernel_repeat_asm(z_asm, x, y, A, n, repeats);

        size_t show = (n < 10) ? n : 10;
        printf("\nAnswers from C code:\nZ = [");
        for (size_t k = 0; k < show; ++k) {
            printf("%.2f", z_c[k]);
            if (k < show - 1) printf(", ");
        }
        printf("]\n");

        printf("Answers from Assembly code:\nZ = [");
        for (size_t k = 0; k < show; ++k) {
            printf("%.2f", z_asm[k]);
            if (k < show - 1) printf(", ");
        }
        printf("]\n");

        int ok = vectors_equal(z_c, z_asm, n, 1e-4f);
        printf("Correctness: %s\n", ok ? "PASS" : "FAIL");

        printf("\nC version average time:   %.6f s\n", t_c);
        printf("ASM version average time: %.6f s\n", t_asm);
        printf("Speedup (C/Assembly):     %.2fx\n", t_c / t_asm);

        aligned_free(x); aligned_free(y); aligned_free(z_c); aligned_free(z_asm);
    }

    printf("\nDone.\n");
    return 0;
}
