#include <assert.h>
#include <stdio.h>
#include <vmem/vmem.h>
// #include "param_slash.c"

vmem_t dummy;

int serial_get(void) {
    return 1234;
}

char invalid_test_vectors[][64] = {
    "set test_array_param[[:3] 1",
    "set test_array_param[: [2 2 2 2 2 2 2 2]",
    "set test_array_param[:3] [1 1 1",
    "set test_array_param 3 3 3 3 3 3 3 3",
    "set test_array_param [3 3 3 3 3 3 3 3",
    "set test_array_param[2:a] 4",
    "set test_array_param [2 3 4 5 6]",
    "set test_array_param[a:b] 3",
    "set test_array_param[a:b] [1 2 4]",
    "set test_array_param[:] [a b c d e f g h]",
};

// Examples of valid inputs:
// char valid_test_vectors[][64] = {
//     "test_array_param[2:5] [1 1 1]",
//     // "2:5] [1 1 1]",
//     // "2:5] 2",
//     // "5 4 5 4 5 4 5 4]",
//     // "-5:7] [1 2 3 4]",
//     // ":4] [6 6 6 6]",
//     "4:] [6 6 6 6]",
//     ":] [0 0 0 0 0 0 0 0]",
// };

char valid_test_vectors[][64] = {
    "2:5",
    "-5:7",
    ":4",
    "4:",
    ":",
};

int param_slash_parse_slice(char * token, int *start_index, int *end_index, int *slice_detected);
int main (int agv, char *argv[]) {
    int start = 0, end = 0, slice_detected = 0;

    for(int i = 0; i < sizeof(invalid_test_vectors)/sizeof(invalid_test_vectors[0]); i++) {
        assert(param_slash_parse_slice(invalid_test_vectors[i], &start, &end, &slice_detected) < 0);
    }
    for(int i = 0; i < sizeof(valid_test_vectors)/sizeof(valid_test_vectors[0]); i++) {
        printf("%s\n", valid_test_vectors[i]);
        fflush(stdout);
        assert(param_slash_parse_slice(valid_test_vectors[i], &start, &end, &slice_detected) == 0);
    }
    return 0;
}