#include <gtest/gtest.h>
#include <apm/environment.h>

TEST(environment, environment_tests) {
    ASSERT_EQ(NULL, csh_getvar("JB"));
    ASSERT_EQ(0, csh_putvar("JB", "SPACEINVENTOR"));
    ASSERT_STREQ("SPACEINVENTOR", csh_getvar("JB"));
    ASSERT_EQ(0, csh_putvar("JB", "JB"));
    ASSERT_STREQ("JB", csh_getvar("JB"));
    char *expanded = csh_expand_vars("$(JB) test $(JB) -v 1 $(JB)");
    ASSERT_STREQ("JB test JB -v 1 JB", expanded);
    free(expanded);
    
    expanded = csh_expand_vars("$(JB) test $(JB) -v 1 $(JB");
    ASSERT_STREQ("JB test JB -v 1 ", expanded);
    free(expanded);
    
    expanded = csh_expand_vars("test -v 1");
    ASSERT_STREQ("test -v 1", expanded);
    free(expanded);

    ASSERT_EQ(0, csh_delvar("JB"));
    ASSERT_EQ(1, csh_delvar("JB"));


    static auto callback_count = 0;
    auto cb = [](const char *name, void *ctx) {
        callback_count++;
    };
    ASSERT_EQ(0, csh_putvar("JB", "SPACEINVENTOR"));
    csh_foreach_var(cb, NULL);
    ASSERT_EQ(1, callback_count);

    callback_count = 0;
    csh_clearenv();
    csh_foreach_var(cb, NULL);
    ASSERT_EQ(0, callback_count);
}
