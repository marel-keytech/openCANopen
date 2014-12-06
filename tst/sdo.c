#include <linux/can.h>

#include "tst.h"
#include "canopen/sdo.h"

#define TEST_DATA_MAX_SIZE 1024

static char _test_data[TEST_DATA_MAX_SIZE];
static int _test_index;
static int _test_subindex;
static size_t _test_size;

void* my_srv_get_sdo_addr(int index, int subindex, size_t* size)
{
	_test_index = index;
	_test_subindex = subindex;
	*size = _test_size;
	return _test_data;
}

int set_get_cs()
{
	struct can_frame frame = { .data = { 0xff } };
	sdo_set_cs(&frame, 0); ASSERT_INT_EQ(0, sdo_get_cs(&frame));
	sdo_set_cs(&frame, 1); ASSERT_INT_EQ(1, sdo_get_cs(&frame));
	sdo_set_cs(&frame, 2); ASSERT_INT_EQ(2, sdo_get_cs(&frame));
	sdo_set_cs(&frame, 3); ASSERT_INT_EQ(3, sdo_get_cs(&frame));
	return 0;
}

int set_get_segment_size()
{
	struct can_frame frame = { .data = { 0xff } };
	sdo_set_segment_size(&frame, 0);
	ASSERT_INT_EQ(0, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 1);
	ASSERT_INT_EQ(1, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 2);
	ASSERT_INT_EQ(2, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 3);
	ASSERT_INT_EQ(3, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 4);
	ASSERT_INT_EQ(4, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 5);
	ASSERT_INT_EQ(5, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 6);
	ASSERT_INT_EQ(6, sdo_get_segment_size(&frame));
	sdo_set_segment_size(&frame, 7);
	ASSERT_INT_EQ(7, sdo_get_segment_size(&frame));
	return 0;
}

int set_get_abort_code()
{
	struct can_frame frame = { 0 };
	sdo_set_abort_code(&frame, 0xdeadbeef);
	ASSERT_INT_EQ(0xdeadbeef, sdo_get_abort_code(&frame));
	return 0;
}

int sdo_srv_dl_init_ok()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 };
	struct can_frame frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	strcpy((char*)&frame_in.data[SDO_MULTIPLEXER_IDX], "ab");
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sdo_srv_dl_sm_init(&sm, &frame_in,
							 &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sm.dl_state);
	ASSERT_INT_EQ(SDO_SCS_DL_INIT_RES, sdo_get_cs(&frame_out));
	ASSERT_STR_EQ("ab", (char*)&frame_out.data[SDO_MULTIPLEXER_IDX]);

	return 0;
}

int sdo_srv_dl_init_failed_cs()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 };
	struct can_frame frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_DL_ABORT, sdo_srv_dl_sm_init(&sm, &frame_in,
							 &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DL_ABORT, sm.dl_state);
	ASSERT_INT_EQ(SDO_SCS_ABORT, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_ABORT_INVALID_CS, sdo_get_abort_code(&frame_out));

	return 0;
}

int sdo_srv_dl_init_remote_abort()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_ABORT);
	ASSERT_INT_EQ(SDO_SRV_DL_INIT, sdo_srv_dl_sm_init(&sm, &frame_in,
							 NULL));
	ASSERT_INT_EQ(SDO_SRV_DL_INIT, sm.dl_state);

	return 0;
}

int sdo_srv_dl_seg_ok()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_DL_SEG_TOGGLED, sdo_srv_dl_sm_seg(&sm, &frame_in,
								&frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG_TOGGLED, sm.dl_state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_FALSE(sdo_is_toggled(&frame_out));
	ASSERT_FALSE(sdo_is_end_segment(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_toggled_ok()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_toggle(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sdo_srv_dl_sm_seg(&sm, &frame_in,
							&frame_out, 1));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sm.dl_state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_TRUE(sdo_is_toggled(&frame_out));
	ASSERT_FALSE(sdo_is_end_segment(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_end_ok()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_end_segment(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_DL_DONE, sdo_srv_dl_sm_seg(&sm, &frame_in,
							 &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_DL_DONE, sm.dl_state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_FALSE(sdo_is_toggled(&frame_out));
	ASSERT_TRUE(sdo_is_end_segment(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_toggled_end_ok()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_toggle(&frame_in);
	sdo_end_segment(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_DL_DONE, sdo_srv_dl_sm_seg(&sm, &frame_in,
							 &frame_out, 1));
	ASSERT_INT_EQ(SDO_SRV_DL_DONE, sm.dl_state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_TRUE(sdo_is_toggled(&frame_out));
	ASSERT_TRUE(sdo_is_end_segment(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_failed_cs()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	ASSERT_INT_EQ(SDO_SRV_DL_ABORT, sdo_srv_dl_sm_seg(&sm, &frame_in,
								&frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_DL_ABORT, sm.dl_state);
	ASSERT_INT_EQ(SDO_SCS_ABORT, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_ABORT_INVALID_CS, sdo_get_abort_code(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_remote_abort()
{
	struct sdo_srv_dl_sm sm = { .dl_state = SDO_SRV_DL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_ABORT);
	ASSERT_INT_EQ(SDO_SRV_DL_REMOTE_ABORT,
		      sdo_srv_dl_sm_seg(&sm, &frame_in, &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_DL_REMOTE_ABORT, sm.dl_state);

	return 0;
}

int sdo_srv_dl_example()
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

int sdo_srv_ul_init_ok()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	strcpy((char*)&frame_in.data[SDO_MULTIPLEXER_IDX], "ab");
	ASSERT_INT_EQ(SDO_SRV_UL_SEG,
		      sdo_srv_ul_sm_init(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_UL_SEG, sm.ul_state);
	ASSERT_INT_EQ(SDO_SCS_UL_INIT_RES, sdo_get_cs(&frame_out));
	ASSERT_STR_EQ("ab", (char*)&frame_out.data[SDO_MULTIPLEXER_IDX]);

	return 0;
}

int sdo_srv_ul_init_failed_cs()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_UL_ABORT,
		      sdo_srv_ul_sm_init(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_UL_ABORT, sm.ul_state);
	ASSERT_INT_EQ(SDO_SCS_ABORT, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_ABORT_INVALID_CS, sdo_get_abort_code(&frame_out));

	return 0;
}

int sdo_srv_ul_init_remote_abort()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_ABORT);
	ASSERT_INT_EQ(SDO_SRV_UL_INIT,
		      sdo_srv_ul_sm_init(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_UL_INIT, sm.ul_state);

	return 0;
}

int sdo_srv_ul_seg_ok()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_UL_SEG_TOGGLED,
		      sdo_srv_ul_sm_seg(&sm, &frame_in, &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_UL_SEG_TOGGLED, sm.ul_state);
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_FALSE(sdo_is_toggled(&frame_out));

	return 0;
}

int sdo_srv_ul_seg_toggled_ok()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	sdo_toggle(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_UL_SEG,
		      sdo_srv_ul_sm_seg(&sm, &frame_in, &frame_out, 1));
	ASSERT_INT_EQ(SDO_SRV_UL_SEG, sm.ul_state);
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_TRUE(sdo_is_toggled(&frame_out));

	return 0;
}

int sdo_srv_ul_seg_failed_cs()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	ASSERT_INT_EQ(SDO_SRV_UL_ABORT,
		      sdo_srv_ul_sm_seg(&sm, &frame_in, &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_UL_ABORT, sm.ul_state);
	ASSERT_INT_EQ(SDO_SCS_ABORT, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_ABORT_INVALID_CS, sdo_get_abort_code(&frame_out));

	return 0;
}

int sdo_srv_ul_seg_remote_abort()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_ABORT);
	ASSERT_INT_EQ(SDO_SRV_UL_REMOTE_ABORT,
		      sdo_srv_ul_sm_seg(&sm, &frame_in, &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_UL_REMOTE_ABORT, sm.ul_state);

	return 0;
}

int sdo_srv_ul_expediated_1byte()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	_test_data[0] = 42;
	_test_size = 1;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	sdo_set_index(&frame_in, 1337);
	sdo_set_subindex(&frame_in, 1);

	ASSERT_INT_EQ(SDO_SRV_UL_DONE, 
		      sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));

	ASSERT_INT_EQ(1337, _test_index);
	ASSERT_INT_EQ(1, _test_subindex);

	ASSERT_TRUE(sdo_is_size_indicated(&frame_out));
	ASSERT_TRUE(sdo_is_expediated(&frame_out));
	ASSERT_INT_EQ(1, sdo_get_expediated_size(&frame_out));
	ASSERT_INT_EQ(42, frame_out.data[SDO_EXPEDIATED_DATA_IDX]);

	return 0;
}

int sdo_srv_ul_expediated_4bytes()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	strcpy(_test_data, "foo");
	_test_size = 4;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	sdo_set_index(&frame_in, 1234);
	sdo_set_subindex(&frame_in, 2);

	ASSERT_INT_EQ(SDO_SRV_UL_DONE, 
		      sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));

	ASSERT_INT_EQ(1234, _test_index);
	ASSERT_INT_EQ(2, _test_subindex);

	ASSERT_TRUE(sdo_is_size_indicated(&frame_out));
	ASSERT_TRUE(sdo_is_expediated(&frame_out));
	ASSERT_INT_EQ(4, sdo_get_expediated_size(&frame_out));
	ASSERT_STR_EQ("foo", (char*)&frame_out.data[SDO_EXPEDIATED_DATA_IDX]);

	return 0;
}

int sdo_srv_ul_segmented_5bytes()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	strcpy(_test_data, "abcd");
	_test_size = 5;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	sdo_set_index(&frame_in, 1000);
	sdo_set_subindex(&frame_in, 3);

	ASSERT_INT_EQ(SDO_SRV_UL_SEG, 
		      sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_INIT_RES, sdo_get_cs(&frame_out));

	ASSERT_INT_EQ(1000, _test_index);
	ASSERT_INT_EQ(3, _test_subindex);

	ASSERT_FALSE(sdo_is_expediated(&frame_out));

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_UL_DONE, 
		      sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_TRUE(sdo_is_end_segment(&frame_out));
	ASSERT_INT_EQ(5, sdo_get_segment_size(&frame_out));
	ASSERT_STR_EQ("abcd", (char*)&frame_out.data[SDO_SEGMENT_IDX]);

	return 0;
}

int sdo_srv_ul_segmented_7bytes()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	strcpy(_test_data, "123456");
	_test_size = 7;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);

	ASSERT_INT_EQ(SDO_SRV_UL_SEG, 
		      sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_INIT_RES, sdo_get_cs(&frame_out));

	ASSERT_FALSE(sdo_is_expediated(&frame_out));

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_UL_DONE, 
		      sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_TRUE(sdo_is_end_segment(&frame_out));
	ASSERT_INT_EQ(7, sdo_get_segment_size(&frame_out));
	ASSERT_STR_EQ("123456", (char*)&frame_out.data[SDO_SEGMENT_IDX]);

	return 0;
}

int sdo_srv_ul_segmented_8bytes()
{
	struct sdo_srv_ul_sm sm = { .ul_state = SDO_SRV_UL_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	memcpy(_test_data, "123456\08", 8);
	_test_size = 8;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);

	ASSERT_INT_EQ(SDO_SRV_UL_SEG, 
		      sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_INIT_RES, sdo_get_cs(&frame_out));

	ASSERT_FALSE(sdo_is_expediated(&frame_out));

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_UL_SEG_TOGGLED, 
		      sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_FALSE(sdo_is_end_segment(&frame_out));
	ASSERT_INT_EQ(7, sdo_get_segment_size(&frame_out));
	ASSERT_STR_EQ("123456", (char*)&frame_out.data[SDO_SEGMENT_IDX]);

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	sdo_toggle(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_UL_DONE, 
		      sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_TRUE(sdo_is_end_segment(&frame_out));
	ASSERT_INT_EQ(1, sdo_get_segment_size(&frame_out));
	ASSERT_INT_EQ('8', frame_out.data[SDO_SEGMENT_IDX]);

	return 0;
}

int main()
{
	int r = 0;
	fprintf(stderr, "Misc:\n");
	RUN_TEST(set_get_cs);
	RUN_TEST(set_get_segment_size);
	RUN_TEST(set_get_abort_code);

	fprintf(stderr, "\nServer download:\n");
	RUN_TEST(sdo_srv_dl_init_ok);
	RUN_TEST(sdo_srv_dl_init_failed_cs);
	RUN_TEST(sdo_srv_dl_init_remote_abort);
	RUN_TEST(sdo_srv_dl_seg_ok);
	RUN_TEST(sdo_srv_dl_seg_toggled_ok);
	RUN_TEST(sdo_srv_dl_seg_end_ok);
	RUN_TEST(sdo_srv_dl_seg_toggled_end_ok);
	RUN_TEST(sdo_srv_dl_seg_failed_cs);
	RUN_TEST(sdo_srv_dl_seg_remote_abort);
	RUN_TEST(sdo_srv_dl_example);

	fprintf(stderr, "\nServer upload state machine:\n");
	sdo_srv_get_sdo_addr = NULL;
	RUN_TEST(sdo_srv_ul_init_ok);
	RUN_TEST(sdo_srv_ul_init_failed_cs);
	RUN_TEST(sdo_srv_ul_init_remote_abort);
	RUN_TEST(sdo_srv_ul_seg_ok);
	RUN_TEST(sdo_srv_ul_seg_toggled_ok);
	RUN_TEST(sdo_srv_ul_seg_failed_cs);
	RUN_TEST(sdo_srv_ul_seg_remote_abort);

	fprintf(stderr, "\nServer upload:\n");
	sdo_srv_get_sdo_addr = my_srv_get_sdo_addr;
	RUN_TEST(sdo_srv_ul_expediated_1byte);
	RUN_TEST(sdo_srv_ul_expediated_4bytes);
	RUN_TEST(sdo_srv_ul_segmented_5bytes);
	RUN_TEST(sdo_srv_ul_segmented_7bytes);
	RUN_TEST(sdo_srv_ul_segmented_8bytes);
	return r;
}


