#include "tst.h"

#include "canopen/error.h"

static int test_cia402_monitoring_torque_too_high()
{
	ASSERT_STR_EQ("Monitoring: Torque: Too high",
		      error_code_to_string(0x8311, 402));
	ASSERT_STR_EQ("Monitoring: Torque",
		      error_code_to_string(0x8300, 402));
	ASSERT_STR_EQ("Monitoring",
		      error_code_to_string(0x8000, 402));
	return 0;
}

static int test_cia302_monitoring_rpdo_timeout()
{
	ASSERT_STR_EQ("Monitoring: Protocol error: RPDO timeout: PDO1",
		      error_code_to_string(0x8251, 302));
	ASSERT_STR_EQ("Monitoring: Protocol error: RPDO timeout",
		      error_code_to_string(0x8250, 302));
	ASSERT_STR_EQ("Monitoring: Protocol error",
		      error_code_to_string(0x8200, 302));
	ASSERT_STR_EQ("Monitoring",
		      error_code_to_string(0x8000, 302));
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_cia402_monitoring_torque_too_high);
	RUN_TEST(test_cia302_monitoring_rpdo_timeout);
	return r;
}
