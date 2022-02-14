#define REGINT(address) *(volatile int*)(address)
#define REGUINT(address) *(volatile unsigned int*)(address)
#define REGINTPOINT(address) (volatile int*)(address)
#define REGFLOAT(address) *(volatile float*)(address)
#define DMEM_BASE  (0xA0030000)

// int main(){
//     // float[2] = float[0] + float[1];
//     float a = REGFLOAT(DMEM_BASE);
//     float b = REGFLOAT(DMEM_BASE+4);
//     REGFLOAT(DMEM_BASE+8) = a * b;
//     REGFLOAT(DMEM_BASE+12) = a - 1.0f;
//     REGFLOAT(DMEM_BASE+16) = a * 2;
//     REGFLOAT(DMEM_BASE+20) = a * 4;
//     REGFLOAT(DMEM_BASE+24) = a < b;
//     REGFLOAT(DMEM_BASE+28) = a > b;
//     REGFLOAT(DMEM_BASE+32) = a >= b;
//     REGFLOAT(DMEM_BASE+36) = a <= b;
// }

// #define //LAPJV_CPP_NEW(x, t, n) if ((x = (t *)malloc(sizeof(t) * (n))) == 0) { return -1; }
// #define //LAPJV_CPP_FREE(x) if (x != 0) { free(x); x = 0; }
#define LAPJV_CPP_SWAP_INDICES(a, b) { int _temp_index = a; a = b; b = _temp_index; }
#define N_MAX 100

#define LARGE 1000000

/** Column-reduction and reduction transfer for a dense cost matrix.
*/
  int _ccrrt_dense(const int n, volatile float* cost,
    int *free_rows, volatile int *x, volatile int *y, float *v)
{
    int n_free_rows;
    // bool *unique;

    for (int i = 0; i < n; i++) {
        x[i] = -1;
        v[i] = LARGE;
        y[i] = 0;
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            const float c = cost[i*n+j];
            if (c < v[j]) {
                v[j] = c;
                y[j] = i;
            }
        }
    }
    REGINT(DMEM_BASE+4*(1+n*n+n*2)) = 1;
    //LAPJV_CPP_NEW(unique, bool, n);
    // memset(unique, true, n);
    bool unique[N_MAX];
    for(int i = 0; i < n; i++) {
        unique[i] = true;
    }
    REGINT(DMEM_BASE+4*(1+n*n+n*2)) = 2;

    {
        int j = n;
        do {
            j--;
            const int i = y[j];
            if (x[i] < 0) {
                x[i] = j;
            }
            else {
                unique[i] = false;
                y[j] = -1;
            }
        } while (j > 0);
    }
    REGINT(DMEM_BASE+4*(1+n*n+n*2)) = 3;
    n_free_rows = 0;
    for (int i = 0; i < n; i++) {
        if (x[i] < 0) {
            free_rows[n_free_rows++] = i;
        }
        else if (unique[i]) {
            const int j = x[i];
            float min = LARGE;
            for (int j2 = 0; j2 < n; j2++) {
                if (j2 == (int)j) {
                    continue;
                }
                const float c = cost[i*n+j2] - v[j2];
                if (c < min) {
                    min = c;
                }
            }
            v[j] -= min;
        }
    }
    //LAPJV_CPP_FREE(unique);
    return n_free_rows;
}


/** Augmenting row reduction for a dense cost matrix.
 */
  int _carr_dense(
    const int n, volatile  float* cost,
    const int n_free_rows,
    int *free_rows, volatile int *x, volatile int *y, float *v)
{
    int current = 0;
    int new_free_rows = 0;
    int rr_cnt = 0;
    while (current < n_free_rows) {
        int i0;
        int j1, j2;
        float v1, v2, v1_new;
        bool v1_lowers;

        rr_cnt++;
        const int free_i = free_rows[current++];
        j1 = 0;
        v1 = cost[free_i*n+0] - v[0];
        j2 = -1;
        v2 = LARGE;
        for (int j = 1; j < n; j++) {
            const float c = cost[free_i*n+j] - v[j];
            if (c < v2) {
                if (c >= v1) {
                    v2 = c;
                    j2 = j;
                }
                else {
                    v2 = v1;
                    v1 = c;
                    j2 = j1;
                    j1 = j;
                }
            }
        }
        i0 = y[j1];
        v1_new = v[j1] - (v2 - v1);
        v1_lowers = v1_new < v[j1];
        if (rr_cnt < current * n) {
            if (v1_lowers) {
                v[j1] = v1_new;
            }
            else if (i0 >= 0 && j2 >= 0) {
                j1 = j2;
                i0 = y[j2];
            }
            if (i0 >= 0) {
                if (v1_lowers) {
                    free_rows[--current] = i0;
                }
                else {
                    free_rows[new_free_rows++] = i0;
                }
            }
        }
        else {
            if (i0 >= 0) {
                free_rows[new_free_rows++] = i0;
            }
        }
        x[free_i] = j1;
        y[j1] = free_i;
    }
    return new_free_rows;
}


/** Find columns with minimum d[j] and put them on the SCAN list.
 */
  int _find_dense(const int n, int lo, float *d, int *cols, volatile  int *y)
{
    int hi = lo + 1;
    float mind = d[cols[lo]];
    for (int k = hi; k < n; k++) {
        int j = cols[k];
        if (d[j] <= mind) {
            if (d[j] < mind) {
                hi = lo;
                mind = d[j];
            }
            cols[k] = cols[hi];
            cols[hi++] = j;
        }
    }
    return hi;
}


// Scan all columns in TODO starting from arbitrary column in SCAN
// and try to decrease d of the TODO columns using the SCAN column.
  int _scan_dense(const int n, volatile  float* cost,
    int *plo, int*phi,
    float *d, int *cols, int *pred,
    volatile int *y, float *v)
{
    int lo = *plo;
    int hi = *phi;
    float h, cred_ij;

    while (lo != hi) {
        int j = cols[lo++];
        const int i = y[j];
        const float mind = d[j];
        h = cost[i*n+j] - v[j] - mind;
        // For all columns in TODO
        for (int k = hi; k < n; k++) {
            j = cols[k];
            cred_ij = cost[i*n+j] - v[j] - h;
            if (cred_ij < d[j]) {
                d[j] = cred_ij;
                pred[j] = i;
                if (cred_ij == mind) {
                    if (y[j] < 0) {
                        return j;
                    }
                    cols[k] = cols[hi];
                    cols[hi++] = j;
                }
            }
        }
    }
    *plo = lo;
    *phi = hi;
    return -1;
}


/** Single iteration of modified Dijkstra shortest path algorithm as explained in the JV paper.
 *
 * This is a dense matrix version.
 *
 * \return The closest free column index.
 */
  int find_path_dense(
    const int n, volatile float* cost,
    const int start_i,
    volatile int *y, float *v,
    int *pred)
{
    int lo = 0, hi = 0;
    int final_j = -1;
    int n_ready = 0;
    // int *cols;
    // float *d;

    //LAPJV_CPP_NEW(cols, int, n);
    //LAPJV_CPP_NEW(d, float, n);
    int cols[N_MAX];
    float d[N_MAX];

    for (int i = 0; i < n; i++) {
        cols[i] = i;
        pred[i] = start_i;
        d[i] = cost[start_i*n+i] - v[i];
    }

    while (final_j == -1) {
        // No columns left on the SCAN list.
        if (lo == hi) {
            n_ready = lo;
            hi = _find_dense(n, lo, d, cols, y);
            for (int k = lo; k < hi; k++) {
                const int j = cols[k];
                if (y[j] < 0) {
                    final_j = j;
                }
            }
        }
        if (final_j == -1) {
            final_j = _scan_dense(
                n, cost, &lo, &hi, d, cols, pred, y, v);
        }
    }

    {
        const float mind = d[cols[lo]];
        for (int k = 0; k < n_ready; k++) {
            const int j = cols[k];
            v[j] += d[j] - mind;
        }
    }

    //LAPJV_CPP_FREE(cols);
    //LAPJV_CPP_FREE(d);

    return final_j;
}


/** Augment for a dense cost matrix.
 */
  int _ca_dense(
    const int n, volatile float* cost,
    const int n_free_rows,
    int *free_rows, volatile int *x, volatile int *y, float *v)
{
    // int *pred;
    int pred[N_MAX];
    //LAPJV_CPP_NEW(pred, int, n);
    for (int *pfree_i = free_rows; pfree_i < free_rows + n_free_rows; pfree_i++) {
        int i = -1, j;
        int k = 0;

        j = find_path_dense(n, cost, *pfree_i, y, v, pred);
        if (j < 0)
        {
            // throw std::runtime_error("Error occured in _ca_dense(): j < 0");
            return -1;
        }
        if (j >= static_cast<int>(n))
        {
            // throw std::runtime_error("Error occured in _ca_dense(): j >= n");
            return -1;
        }
        while (i != *pfree_i) {
            i = pred[j];
            y[j] = i;
            LAPJV_CPP_SWAP_INDICES(j, x[i]);
            k++;
            if (k >= n) {
                // throw std::runtime_error("Error occured in _ca_dense(): k >= n");
                return -1;
            }
        }
    }
    //LAPJV_CPP_FREE(pred);
    return 0;
}

/** Solve dense sparse LAP. */
  int lapjv_internal(
    const int n, volatile float* cost,
    volatile int *x, volatile  int *y)
{
    int ret;
    // int *free_rows;
    // float *v;
    int free_rows[N_MAX];
    float v[N_MAX];
    //LAPJV_CPP_NEW(free_rows, int, n);
    //LAPJV_CPP_NEW(v, float, n);
    ret = _ccrrt_dense(n, cost, free_rows, x, y, v);

    int i = 0;
    while (ret > 0 && i < 2) {
        ret = _carr_dense(n, cost, ret, free_rows, x, y, v);
        i++;
    }
    if (ret > 0) {
        ret = _ca_dense(n, cost, ret, free_rows, x, y, v);
    }
    //LAPJV_CPP_FREE(v);
    //LAPJV_CPP_FREE(free_rows);
    return ret;
}


int main(){

    int n = REGINT(DMEM_BASE);
    //start flag
    REGINT(DMEM_BASE+4*(1+n*n+n*2)) = n;

    volatile float cost[N_MAX*N_MAX];
    for(int i = 0; i < n*n; i++) cost[i] = REGFLOAT(DMEM_BASE+4*(1+i));
    volatile int* x = REGINTPOINT(DMEM_BASE+4*(1+n*n));
	volatile int* y = REGINTPOINT(DMEM_BASE+4*(1+n*n+n));
    int ret = lapjv_internal(n, cost, x, y);

    //end flag
    REGINT(DMEM_BASE+4*(1+n*n+n*2)) = n*2;
    // REGINT(DMEM_BASE+4*(1+n*n+n*2+1)) = ret;
    //
    while(1){

    }
    return 1;

}


// int main(){
//     // float[2] = float[0] + float[1];
//     float a = REGFLOAT(DMEM_BASE);
//     float b = REGFLOAT(DMEM_BASE+4);
//     REGFLOAT(DMEM_BASE+8) = a*b;
//     REGFLOAT(DMEM_BASE+12) = a - 1.0f;
//     REGFLOAT(DMEM_BASE+16) = a * 2;
//     REGFLOAT(DMEM_BASE+20) = a * 4;
//     REGFLOAT(DMEM_BASE+24) = a < b;
//     REGFLOAT(DMEM_BASE+28) = a > b;
//     REGFLOAT(DMEM_BASE+32) = a >= b;
//     REGFLOAT(DMEM_BASE+36) = a <= b;
//     while(1){

//     }
//     return 1;
// }
