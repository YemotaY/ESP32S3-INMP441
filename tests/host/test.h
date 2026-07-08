/* Tiny dependency-free unit-test harness. */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        g_checks++;                                                            \
        if (!(cond)) {                                                         \
            g_fails++;                                                         \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                      \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                     \
    do {                                                                       \
        long _a = (long)(a), _b = (long)(b);                                   \
        g_checks++;                                                            \
        if (_a != _b) {                                                        \
            g_fails++;                                                         \
            printf("  FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
                   #a, _a, #b, _b);                                            \
        }                                                                      \
    } while (0)

#define CHECK_STR_EQ(a, b)                                                     \
    do {                                                                       \
        const char *_sa = (a), *_sb = (b);                                     \
        g_checks++;                                                            \
        if (strcmp(_sa, _sb) != 0) {                                           \
            g_fails++;                                                         \
            printf("  FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__,     \
                   _sa, _sb);                                                  \
        }                                                                      \
    } while (0)

#define RUN(fn)                                                                \
    do {                                                                       \
        printf("- %s\n", #fn);                                                 \
        fn();                                                                  \
    } while (0)

#define TEST_MAIN_BEGIN(name) int main(void) { printf("== %s ==\n", (name));
#define TEST_MAIN_END()                                                        \
    printf("%s: %d checks, %d failures\n", (g_fails ? "FAILED" : "ok"),        \
           g_checks, g_fails);                                                 \
    return g_fails ? 1 : 0;                                                     \
    }

#endif /* TEST_H */
