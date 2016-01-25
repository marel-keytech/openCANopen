#include "tst.h"

#include "canopen/sdo-dict.h"

static int test_sdo_dict_tostring(void)
{
	ASSERT_STR_EQ("device-type", sdo_dict_tostring(SDO_MUX(0x1000, 0)));
	ASSERT_STR_EQ("time-cob-id", sdo_dict_tostring(SDO_MUX(0x1012, 0)));
	ASSERT_STR_EQ("product-code", sdo_dict_tostring(SDO_MUX(0x1018, 2)));
	return 0;
}

static int test_sdo_dict_fromstring(void)
{
	ASSERT_UINT_EQ(SDO_MUX(0x1001, 0), sdo_dict_fromstring("error-register"));
	ASSERT_UINT_EQ(SDO_MUX(0x1018, 1), sdo_dict_fromstring("vendor-id"));
	return 0;
}

static int test_sdo_dict_type(void)
{
	ASSERT_UINT_EQ(CANOPEN_UNSIGNED32, sdo_dict_type(SDO_MUX(0x1000, 0)));
	ASSERT_UINT_EQ(CANOPEN_VISIBLE_STRING, sdo_dict_type(SDO_MUX(0x1008, 0)));
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_sdo_dict_tostring);
	RUN_TEST(test_sdo_dict_fromstring);
	RUN_TEST(test_sdo_dict_type);
	return r;
}

