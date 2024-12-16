#include <gtest/gtest.h>
#include "require_version.h"

TEST(require_version, require_version_tests) {

    version_t version = {0};

    #define STRINGIFY(x) str(x)
    #define str(x) #x
    #define MAJOR 3
    #define MINOR 5
    #define PATCH 27
    #define STR_MAJOR STRINGIFY(MAJOR)
    #define STR_MINOR STRINGIFY(MINOR)
    #define STR_PATCH STRINGIFY(PATCH)
    
    ASSERT_TRUE(parse_version(STR_MAJOR "." STR_MINOR "." STR_PATCH, &version));

    /* Version parsed successfully, now check if it was also done correctly. */
    ASSERT_EQ(version.major, MAJOR);
    ASSERT_EQ(version.minor, MINOR);
    ASSERT_EQ(version.patch, PATCH);

    /* We currently also allow dashes */
    ASSERT_TRUE(parse_version(STR_MAJOR "." STR_MINOR "-" STR_PATCH, &version));

    /* So that should also be parsed correctly */
    ASSERT_EQ(version.major, MAJOR);
    ASSERT_EQ(version.minor, MINOR);
    ASSERT_EQ(version.patch, PATCH);

    /* Let's also test our version constraints */
    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "." STR_MINOR "." STR_PATCH));
    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "-" STR_MINOR "-" STR_PATCH));

    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "." STR_MINOR ".*"));
    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "-" STR_MINOR "-*"));

    ASSERT_FALSE(compare_version(&version, "==" STRINGIFY(MAJOR-1) "." STR_MINOR ".*"));
    ASSERT_FALSE(compare_version(&version, "==" STRINGIFY(MAJOR-1) "-" STR_MINOR "-*"));

    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR ".*"));
    ASSERT_TRUE(compare_version(&version, "==" STR_MAJOR "-*"));


    ASSERT_TRUE(compare_version(&version, ">=" STR_MAJOR "." STR_MINOR "." STR_PATCH));
    ASSERT_TRUE(compare_version(&version, ">=" STR_MAJOR "-" STR_MINOR "-" STR_PATCH));

    ASSERT_TRUE(compare_version(&version, ">=" STR_MAJOR "." STR_MINOR ".*"));
    ASSERT_TRUE(compare_version(&version, ">=" STR_MAJOR "-" STR_MINOR "-*"));


    ASSERT_FALSE(compare_version(&version, ">" STR_MAJOR "." STR_MINOR ".*"));
    ASSERT_FALSE(compare_version(&version, ">" STR_MAJOR "-" STR_MINOR "-*"));

    ASSERT_TRUE(compare_version(&version, ">" STR_MAJOR "." STR_MINOR "." STRINGIFY(PATCH-1)));
    ASSERT_TRUE(compare_version(&version, ">" STR_MAJOR "-" STR_MINOR "-" STRINGIFY(PATCH-1)));

    ASSERT_TRUE(compare_version(&version, ">" STR_MAJOR "." STRINGIFY(MINOR-1) "." STR_PATCH));
    ASSERT_TRUE(compare_version(&version, ">" STR_MAJOR "-" STRINGIFY(MINOR-1) "-" STR_PATCH));


    ASSERT_TRUE(compare_version(&version, "<=" STR_MAJOR "." STR_MINOR "." STR_PATCH));
    ASSERT_TRUE(compare_version(&version, "<=" STR_MAJOR "-" STR_MINOR "-" STR_PATCH));

    ASSERT_TRUE(compare_version(&version, "<=" STR_MAJOR "." STR_MINOR ".*"));
    ASSERT_TRUE(compare_version(&version, "<=" STR_MAJOR "-" STR_MINOR "-*"));


    ASSERT_FALSE(compare_version(&version, "<" STR_MAJOR "." STR_MINOR ".*"));
    ASSERT_FALSE(compare_version(&version, "<" STR_MAJOR "-" STR_MINOR "-*"));

    ASSERT_TRUE(compare_version(&version, "<" STR_MAJOR "." STR_MINOR "." STRINGIFY(PATCH+1)));
    ASSERT_TRUE(compare_version(&version, "<" STR_MAJOR "-" STR_MINOR "-" STRINGIFY(PATCH+1)));

    ASSERT_TRUE(compare_version(&version, "<" STR_MAJOR "." STRINGIFY(MINOR+1) "." STR_PATCH));
    ASSERT_TRUE(compare_version(&version, "<" STR_MAJOR "-" STRINGIFY(MINOR+1) "-" STR_PATCH));


    ASSERT_FALSE(compare_version(&version, "!=" STR_MAJOR "." STR_MINOR "." STR_PATCH));
    ASSERT_FALSE(compare_version(&version, "!=" STR_MAJOR "-*"));

    ASSERT_TRUE(compare_version(&version, "!=" STRINGIFY(MAJOR+1) "." STR_MINOR "." STR_PATCH));
    ASSERT_TRUE(compare_version(&version, "!=" STRINGIFY(MAJOR+1) "-*"));
}
