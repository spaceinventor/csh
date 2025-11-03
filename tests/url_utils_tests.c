#include <assert.h>
#include <stddef.h>
#include "url_utils.h"

static void is_http_url_tests() {
    const char *vector_1 = "I am not an URL";
    assert(is_http_url(vector_1) == false);

    const char *vector_2 = "";
    assert(is_http_url(vector_2) == false);

    const char *vector_3 = NULL;
    assert(is_http_url(vector_3) == false);

    const char *vector_4 = NULL;
    assert(is_http_url(vector_4) == false);

    const char *vector_5 = "http://";
    assert(is_http_url(vector_5) == false);

    const char *vector_6 = "https://";
    assert(is_http_url(vector_6) == false);

    const char *vector_7 = "https://a";
    assert(is_http_url(vector_7) == false);

    const char *vector_8 = "http://a";
    assert(is_http_url(vector_8) == false);

    const char *vector_9 = "http://a.";
    assert(is_http_url(vector_9) == false);

    const char *vector_10 = "http://a.b";
    assert(is_http_url(vector_10) == true);

    const char *vector_11 = "https://a.b";
    assert(is_http_url(vector_11) == true);

    const char *vector_12 = "https://a";
    assert(is_http_url(vector_12) == false);

    const char *vector_13 = "https://a.";
    assert(is_http_url(vector_13) == false);
}

int main(int argc, const char *argv[]) {
    int status = 0;
    is_http_url_tests();
    return status;
}