#include <check.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char btPort;
} ODPortInfo;

extern void ODComFormatDeviceName(char *szDevName, size_t size, const ODPortInfo *pPortInfo);

START_TEST(test_device_name_buffer_boundary)
{
    // Invariant: Device name formatting must never overflow the destination buffer
    // regardless of port value, maintaining memory safety at the security boundary
    
    typedef struct {
        unsigned char port;
        const char *description;
    } TestCase;
    
    TestCase cases[] = {
        {255, "maximum_port_value"},
        {0, "minimum_port_value"},
        {99, "typical_valid_port"}
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);
    
    for (int i = 0; i < num_cases; i++) {
        char buffer[8];
        memset(buffer, 0xAA, sizeof(buffer));
        
        ODPortInfo portInfo;
        portInfo.btPort = cases[i].port;
        
        ODComFormatDeviceName(buffer, sizeof(buffer), &portInfo);
        
        // Verify null termination within bounds
        ck_assert_msg(buffer[7] == '\0' || strlen(buffer) < 8,
                      "Buffer overflow detected for port %u (%s)",
                      cases[i].port, cases[i].description);
        
        // Verify expected format
        char expected[8];
        snprintf(expected, sizeof(expected), "COM%u", (unsigned)cases[i].port + 1);
        ck_assert_str_eq(buffer, expected);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_device_name_buffer_boundary);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}