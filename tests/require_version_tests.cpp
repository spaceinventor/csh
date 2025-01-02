#include <gtest/gtest.h>
#include "require_version.h"

TEST(require_version, require_version_tests) {

    

    #define STRINGIFY(x) str(x)
    #define str(x) #x
    #define MAJOR 3
    #define MINOR 5
    #define PATCH 27
    #define STR_MAJOR STRINGIFY(MAJOR)
    #define STR_MINOR STRINGIFY(MINOR)
    #define STR_PATCH STRINGIFY(PATCH)
    
    {   /* Testing parse_version(), while also using it to generate the version_t for compare_version. */

        version_t version = {0};

        /* Here are some things that are not valid version specifiers */
        ASSERT_FALSE(parse_version("vv3.4.5", &version, false));
        ASSERT_FALSE(parse_version("3..4.5", &version, false));
        ASSERT_FALSE(parse_version("3.4.5.3", &version, false));
        ASSERT_FALSE(parse_version("3.", &version, false));
        ASSERT_FALSE(parse_version("3.d", &version, false));
        ASSERT_FALSE(parse_version("3.v", &version, false));
        ASSERT_FALSE(parse_version("200000000000000000000000000000.200000000000000000000000000000.200000000000000000000000000000", &version, false));  // Long overflow
        ASSERT_FALSE(parse_version("2147483648.2147483648.2147483648", &version, false));  // int32 overflow

        /* It should be permissable to leave out certain parts of the version. */
        ASSERT_TRUE(parse_version("v3", &version, false));
        /* In such cases the remaining fields should be zeroed. */
        ASSERT_EQ(version.major, 3);
        ASSERT_EQ(version.minor, 0);
        ASSERT_EQ(version.patch, 0);

        ASSERT_TRUE(parse_version("4", &version, false));
        ASSERT_EQ(version.major, 4);
        ASSERT_EQ(version.minor, 0);
        ASSERT_EQ(version.patch, 0);

        ASSERT_TRUE(parse_version("5.3", &version, false));
        ASSERT_EQ(version.major, 5);
        ASSERT_EQ(version.minor, 3);
        ASSERT_EQ(version.patch, 0);

        ASSERT_TRUE(parse_version(STR_MAJOR "." STR_MINOR "." STR_PATCH, &version, false));
        /* Version parsed successfully, now check if it was also done correctly. */
        ASSERT_EQ(version.major, MAJOR);
        ASSERT_EQ(version.minor, MINOR);
        ASSERT_EQ(version.patch, PATCH);

        /* We currently also allow dashes */
        ASSERT_TRUE(parse_version(STR_MAJOR "." STR_MINOR "-" STR_PATCH, &version, false));
        /* So that should also be parsed correctly */
        ASSERT_EQ(version.major, MAJOR);
        ASSERT_EQ(version.minor, MINOR);
        ASSERT_EQ(version.patch, PATCH);

        /* Check that it works with 'v' prefix as well */
        ASSERT_TRUE(parse_version("v" STR_MAJOR "." STR_MINOR "-" STR_PATCH, &version, false));
        ASSERT_EQ(version.major, MAJOR);
        ASSERT_EQ(version.minor, MINOR);
        ASSERT_EQ(version.patch, PATCH);

        /* Test an actual CSH version */
        ASSERT_TRUE(parse_version("2.5-36-g3b8a51f+", &version, true));
        ASSERT_EQ(version.major, 2);
        ASSERT_EQ(version.minor, 5);
        ASSERT_EQ(version.patch, 36);

        ASSERT_TRUE(parse_version("3.4.5.3", &version, true));
        ASSERT_EQ(version.major, 3);
        ASSERT_EQ(version.minor, 4);
        ASSERT_EQ(version.patch, 5);
    
    }

    /* We could reuse the version from parse_version().
        But explicitly recreating here allows us to make it const. */
    const version_t version = {MAJOR, MINOR, PATCH};

    /* Let's also test our version constraints */
    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "." STR_MINOR "." STR_PATCH, false));
    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "-" STR_MINOR "-" STR_PATCH, false));

    /* Operator should default to "==" */
    ASSERT_TRUE(compare_version(&version, STR_MAJOR "." STR_MINOR "." STR_PATCH, false));
    ASSERT_TRUE(compare_version(&version, STR_MAJOR "-" STR_MINOR "-" STR_PATCH, false));

    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "." STR_MINOR ".*", false));
    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "-" STR_MINOR "-*", false));

    ASSERT_FALSE(compare_version(&version, "==" STRINGIFY(MAJOR-1) "." STR_MINOR ".*", false));
    ASSERT_FALSE(compare_version(&version, "==" STRINGIFY(MAJOR-1) "-" STR_MINOR "-*", false));

    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR ".*", false));
    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "-*", false));


    ASSERT_TRUE(compare_version(&version, ">=" STR_MAJOR "." STR_MINOR "." STR_PATCH, false));
    ASSERT_TRUE(compare_version(&version, ">=" STR_MAJOR "-" STR_MINOR "-" STR_PATCH, false));

    ASSERT_TRUE(compare_version(&version, ">=" STR_MAJOR "." STR_MINOR ".*", false));
    ASSERT_TRUE(compare_version(&version, ">=" STR_MAJOR "-" STR_MINOR "-*", false));


    ASSERT_FALSE(compare_version(&version, ">" STR_MAJOR "." STR_MINOR ".*", false));
    ASSERT_FALSE(compare_version(&version, ">" STR_MAJOR "-" STR_MINOR "-*", false));

    static_assert(PATCH-1 == 26);  /* TODO: How to stringify PATCH-1 ? */
    ASSERT_TRUE(compare_version(&version, ">" STR_MAJOR "." STR_MINOR "." "26", false));
    ASSERT_TRUE(compare_version(&version, ">" STR_MAJOR "-" STR_MINOR "-" "26", false));

    static_assert(MINOR-1 == 4);  /* TODO: How to stringify MINOR-1 ? */
    ASSERT_TRUE(compare_version(&version, ">" STR_MAJOR "." "4" "." STR_PATCH, false));
    ASSERT_TRUE(compare_version(&version, ">" STR_MAJOR "-" "4" "-" STR_PATCH, false));


    ASSERT_TRUE(compare_version(&version, "<=" STR_MAJOR "." STR_MINOR "." STR_PATCH, false));
    ASSERT_TRUE(compare_version(&version, "<=" STR_MAJOR "-" STR_MINOR "-" STR_PATCH, false));

    ASSERT_TRUE(compare_version(&version, "<=" STR_MAJOR "." STR_MINOR ".*", false));
    ASSERT_TRUE(compare_version(&version, "<=" STR_MAJOR "-" STR_MINOR "-*", false));


    ASSERT_FALSE(compare_version(&version, "<" STR_MAJOR "." STR_MINOR ".*", false));
    ASSERT_FALSE(compare_version(&version, "<" STR_MAJOR "-" STR_MINOR "-*", false));

    static_assert(PATCH+1 == 28);  /* TODO: How to stringify PATCH+1 ? */
    ASSERT_TRUE(compare_version(&version, "<" STR_MAJOR "." STR_MINOR "." "28", false));
    ASSERT_TRUE(compare_version(&version, "<" STR_MAJOR "-" STR_MINOR "-" "28", false));

    static_assert(MINOR+1 == 6);  /* TODO: How to stringify MINOR+1 ? */
    ASSERT_TRUE(compare_version(&version, "<" STR_MAJOR "." "6" "." STR_PATCH, false));
    ASSERT_TRUE(compare_version(&version, "<" STR_MAJOR "-" "6" "-" STR_PATCH, false));


    ASSERT_FALSE(compare_version(&version, "!=" STR_MAJOR "." STR_MINOR "." STR_PATCH, false));
    ASSERT_FALSE(compare_version(&version, "!=" STR_MAJOR "-*", false));

    static_assert(MAJOR+1 == 4);  /* TODO: How to stringify MAJOR+1 ? */
    ASSERT_TRUE(compare_version(&version, "!=" "4" "." STR_MINOR "." STR_PATCH, false));
    ASSERT_TRUE(compare_version(&version, "!=" "4" "-*", false));


    /* Unknown operator test case */
    ASSERT_FALSE(compare_version(&version, "<<" STR_MAJOR "." STR_MINOR "." STR_PATCH, false));
    ASSERT_FALSE(compare_version(&version, "<<" STR_MAJOR "-" STR_MINOR "-" STR_PATCH, false));

    /* Too many operator characters */
    ASSERT_FALSE(compare_version(&version, "====" STR_MAJOR "." STR_MINOR "." STR_PATCH, false));
    ASSERT_FALSE(compare_version(&version, "====" STR_MAJOR "-" STR_MINOR "-" STR_PATCH, false));

    /* Too many version parts */
    ASSERT_FALSE(compare_version(&version, STR_MAJOR "." STR_MINOR "." STR_PATCH "." STR_PATCH, false));
    ASSERT_FALSE(compare_version(&version, STR_MAJOR "-" STR_MINOR "-" STR_PATCH "-" STR_PATCH, false));

    /* NOTE: Currently we have a maximum string length we will parse,
        but this could be considered an implementation detail. */
    ASSERT_FALSE(compare_version(&version, "2094967295.2094967295.2094967295", false));
    ASSERT_FALSE(compare_version(&version, "2094967295-2094967295-2094967295", false));
}
