#include <stdio.h>
#include "banker.h"

#define NP 5   /* processes */
#define NR 3   /* resource types A, B, C */

static const int ALLOCATION[NP][NR] = {
    { 0, 1, 0 }, { 2, 0, 0 }, { 3, 0, 2 }, { 2, 1, 1 }, { 0, 0, 2 },
};
static const int MAXIMUM[NP][NR] = {
    { 7, 5, 3 }, { 3, 2, 2 }, { 9, 0, 2 }, { 2, 2, 2 }, { 4, 3, 3 },
};
static const int AVAILABLE_SAFE[NR]   = { 3, 3, 2 };
static const int AVAILABLE_UNSAFE[NR] = { 0, 1, 1 };

static void print_vec(const int v[NR])
{
    printf("(%d %d %d)", v[0], v[1], v[2]);
}

/* Fills seq with the processes that can finish, in order. Returns 1 if all NP can. */
static int is_safe_state(const int avail[NR], const int alloc[NP][NR], const int max[NP][NR],
                         int seq[NP], int *seq_len)
{
    int work[NR], need[NP][NR], finish[NP] = { 0 }, count = 0, progress = 1;

    for (int r = 0; r < NR; r++)
        work[r] = avail[r];
    for (int i = 0; i < NP; i++)
        for (int r = 0; r < NR; r++)
            need[i][r] = max[i][r] - alloc[i][r];

    while (progress) {
        progress = 0;
        for (int i = 0; i < NP; i++) {
            if (finish[i])
                continue;
            int fits = 1;
            for (int r = 0; r < NR; r++)
                if (need[i][r] > work[r])
                    fits = 0;
            if (!fits)
                continue;

            printf("  P%d: Need ", i);
            print_vec(need[i]);
            printf(" <= Work ");
            print_vec(work);
            for (int r = 0; r < NR; r++)
                work[r] += alloc[i][r];
            printf(" -> runs, releases Allocation, Work = ");
            print_vec(work);
            printf("\n");

            finish[i] = 1;
            seq[count++] = i;
            progress = 1;
        }
    }
    *seq_len = count;
    return count == NP;
}

int run_banker_demo(int unsafe_case)
{
    const int *avail = unsafe_case ? AVAILABLE_UNSAFE : AVAILABLE_SAFE;
    int seq[NP], len;

    printf("=== Banker's safety algorithm: %s test case ===\n", unsafe_case ? "UNSAFE" : "SAFE");
    printf("Available (A B C): ");
    print_vec(avail);
    printf("\n\n  Proc  Allocation   Maximum    Need = Max - Alloc\n");
    for (int i = 0; i < NP; i++) {
        int need[NR];
        for (int r = 0; r < NR; r++)
            need[r] = MAXIMUM[i][r] - ALLOCATION[i][r];
        printf("  P%d    ", i);
        print_vec(ALLOCATION[i]);
        printf("     ");
        print_vec(MAXIMUM[i]);
        printf("    ");
        print_vec(need);
        printf("\n");
    }

    printf("\nSafety check:\n");
    int safe = is_safe_state(avail, ALLOCATION, MAXIMUM, seq, &len);

    if (safe) {
        printf("\nResult: SAFE. Safe sequence: ");
        for (int k = 0; k < len; k++)
            printf("P%d%s", seq[k], k < len - 1 ? " -> " : "\n");
    } else {
        int finished[NP] = { 0 };
        for (int k = 0; k < len; k++)
            finished[seq[k]] = 1;
        printf("%s\nResult: UNSAFE. No safe sequence exists.\nProcesses that can never finish:",
               len ? "" : "  no process's Need fits in Work\n");
        for (int i = 0; i < NP; i++)
            if (!finished[i])
                printf(" P%d", i);
        printf("\n");
    }
    return safe == !unsafe_case ? 0 : -1;
}
