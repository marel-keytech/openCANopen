#include <linux/can.h>

#include "tst.h"
#include "canopen/sdo.h"

int set_get_cs()
{
	struct can_frame frame = { .data = 0xff };
	sdo_set_cs(&frame, 0); ASSERT_INT_EQ(0, sdo_get_cs(&frame));
	sdo_set_cs(&frame, 1); ASSERT_INT_EQ(1, sdo_get_cs(&frame));
	sdo_set_cs(&frame, 2); ASSERT_INT_EQ(2, sdo_get_cs(&frame));
	sdo_set_cs(&frame, 3); ASSERT_INT_EQ(3, sdo_get_cs(&frame));
	return 0;
}

int set_get_segment_size()
{
	struct can_frame frame = { .data = 0xff };
	sdo_set_segment_size(&frame, 0); ASSERT_INT_EQ(0, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 1); ASSERT_INT_EQ(1, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 2); ASSERT_INT_EQ(2, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 3); ASSERT_INT_EQ(3, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 4); ASSERT_INT_EQ(4, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 5); ASSERT_INT_EQ(5, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 6); ASSERT_INT_EQ(6, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 7); ASSERT_INT_EQ(7, sdo_get_segment_size(&frame));
	return 0;
}

int sdo_srv_dl_no_size()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 };
	struct can_frame frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	strcpy((char*)&frame_in.data[SDO_MULTIPLEXER_IDX], "ab");

	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sdo_srv_dl_sm_feed(&sm, &frame_in,
							 &frame_out));
	ASSERT_INT_EQ(SDO_SCS_DL_INIT_RES, sdo_get_cs(&frame_out));
	ASSERT_STR_EQ("ab", (char*)&frame_out.data[SDO_MULTIPLEXER_IDX]);

	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_DL_SEG_TOGGLED, sdo_srv_dl_sm_feed(&sm, &frame_in,
								 &frame_out));
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_FALSE(sdo_is_toggled(&frame_out));
	ASSERT_FALSE(sdo_is_end_segment(&frame_out));

	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_toggle(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sdo_srv_dl_sm_feed(&sm, &frame_in,
							 &frame_out));
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_TRUE(sdo_is_toggled(&frame_out));
	ASSERT_FALSE(sdo_is_end_segment(&frame_out));

	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_toggle(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_DL_SEG_TOGGLED, sdo_srv_dl_sm_feed(&sm, &frame_in,
								 &frame_out));
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_FALSE(sdo_is_toggled(&frame_out));
	ASSERT_FALSE(sdo_is_end_segment(&frame_out));

	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_toggle(&frame_in);
	sdo_end_segment(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_DL_DONE, sdo_srv_dl_sm_feed(&sm, &frame_in,
							  &frame_out));
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_TRUE(sdo_is_toggled(&frame_out));
	ASSERT_TRUE(sdo_is_end_segment(&frame_out));
	
	return 0;
}


int main()
{
	int r = 0;
	RUN_TEST(set_get_cs);
	RUN_TEST(set_get_segment_size);
	RUN_TEST(sdo_srv_dl_no_size);
	return r;
}


