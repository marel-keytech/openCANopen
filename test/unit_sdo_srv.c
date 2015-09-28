#include <sys/socket.h>
#include <linux/can.h>

#include "tst.h"
#include "canopen/sdo.h"
#include "canopen/sdo_srv.h"

#define TEST_DATA_MAX_SIZE 1024

static char _test_data[TEST_DATA_MAX_SIZE];
static int _test_index;
static int _test_subindex;
static size_t _test_size;

int my_sdo_get_obj(struct sdo_obj* obj, int index, int subindex)
{
	_test_index = index;
	_test_subindex = subindex;
	obj->addr = _test_data;
	obj->size = _test_size;
	obj->flags = SDO_OBJ_RW | SDO_OBJ_EQ;
	return 0;
}

int set_get_cs()
{
	struct can_frame frame = { .data = { 0xff } };
	sdo_set_cs(&frame, 0); ASSERT_UINT_EQ(0, sdo_get_cs(&frame));
	sdo_set_cs(&frame, 1); ASSERT_UINT_EQ(1, sdo_get_cs(&frame));
	sdo_set_cs(&frame, 2); ASSERT_UINT_EQ(2, sdo_get_cs(&frame));
	sdo_set_cs(&frame, 3); ASSERT_UINT_EQ(3, sdo_get_cs(&frame));
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
	ASSERT_UINT_EQ(0xdeadbeef, sdo_get_abort_code(&frame));
	return 0;
}

int sdo_srv_dl_init_ok()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 };
	struct can_frame frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	strcpy((char*)&frame_in.data[SDO_MULTIPLEXER_IDX], "ab");
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sdo_srv_dl_sm_init(&sm, &frame_in,
							 &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_INIT_RES, sdo_get_cs(&frame_out));
	ASSERT_STR_EQ("ab", (char*)&frame_out.data[SDO_MULTIPLEXER_IDX]);

	return 0;
}

int sdo_srv_dl_init_failed_cs()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 };
	struct can_frame frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_ABORT, sdo_srv_dl_sm_init(&sm, &frame_in,
							 &frame_out));
	ASSERT_INT_EQ(SDO_SRV_ABORT, sm.state);
	ASSERT_INT_EQ(SDO_SCS_ABORT, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_ABORT_INVALID_CS, sdo_get_abort_code(&frame_out));

	return 0;
}

int sdo_srv_dl_init_remote_abort()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_ABORT);
	ASSERT_INT_EQ(SDO_SRV_INIT, sdo_srv_dl_sm_init(&sm, &frame_in,
							 NULL));
	ASSERT_INT_EQ(SDO_SRV_INIT, sm.state);

	return 0;
}

int sdo_srv_dl_seg_ok()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_DL_SEG_TOGGLED, sdo_srv_dl_sm_seg(&sm, &frame_in,
								&frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG_TOGGLED, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_FALSE(sdo_is_toggled(&frame_out));
	ASSERT_FALSE(sdo_is_end_segment(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_toggled_ok()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_toggle(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sdo_srv_dl_sm_seg(&sm, &frame_in,
							&frame_out, 1));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_TRUE(sdo_is_toggled(&frame_out));
	ASSERT_FALSE(sdo_is_end_segment(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_end_ok()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_end_segment(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_DONE, sdo_srv_dl_sm_seg(&sm, &frame_in,
							 &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_FALSE(sdo_is_toggled(&frame_out));
	ASSERT_TRUE(sdo_is_end_segment(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_toggled_end_ok()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_toggle(&frame_in);
	sdo_end_segment(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_DONE, sdo_srv_dl_sm_seg(&sm, &frame_in,
							 &frame_out, 1));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_TRUE(sdo_is_toggled(&frame_out));
	ASSERT_TRUE(sdo_is_end_segment(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_failed_cs()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	ASSERT_INT_EQ(SDO_SRV_ABORT, sdo_srv_dl_sm_seg(&sm, &frame_in,
								&frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_ABORT, sm.state);
	ASSERT_INT_EQ(SDO_SCS_ABORT, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_ABORT_INVALID_CS, sdo_get_abort_code(&frame_out));

	return 0;
}

int sdo_srv_dl_seg_remote_abort()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };
	
	sdo_set_cs(&frame_in, SDO_CCS_ABORT);
	ASSERT_INT_EQ(SDO_SRV_REMOTE_ABORT,
		      sdo_srv_dl_sm_seg(&sm, &frame_in, &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_REMOTE_ABORT, sm.state);

	return 0;
}

int sdo_srv_dl_example()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
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
	ASSERT_INT_EQ(SDO_SRV_DONE, sdo_srv_dl_sm_feed(&sm, &frame_in,
							  &frame_out));
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_TRUE(sdo_is_toggled(&frame_out));
	ASSERT_TRUE(sdo_is_end_segment(&frame_out));
	
	return 0;
}

int sdo_srv_dl_expediated_1byte()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	memset(_test_data, -1, TEST_DATA_MAX_SIZE);
	_test_size = 1;

	sdo_expediate(&frame_in);
	sdo_indicate_size(&frame_in);
	sdo_set_expediated_size(&frame_in, 1);
	frame_in.data[SDO_EXPEDIATED_DATA_IDX] = 42;

	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	ASSERT_INT_GE(0, sdo_srv_dl_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_INIT_RES, sdo_get_cs(&frame_out));

	ASSERT_UINT_EQ(42, _test_data[0]);
	ASSERT_INT_EQ(-1, _test_data[1]);

	return 0;
}

int sdo_srv_dl_expediated_4bytes()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	memset(_test_data, -1, TEST_DATA_MAX_SIZE);
	_test_size = 4;

	sdo_expediate(&frame_in);
	sdo_set_expediated_size(&frame_in, 1);
	strcpy((char*)&frame_in.data[SDO_EXPEDIATED_DATA_IDX], "foo");

	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	ASSERT_INT_GE(0, sdo_srv_dl_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_INIT_RES, sdo_get_cs(&frame_out));

	ASSERT_STR_EQ("foo", _test_data);

	return 0;
}

int sdo_srv_dl_segmented_5bytes()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	memset(_test_data, -1, TEST_DATA_MAX_SIZE);
	_test_size = 5;

	sdo_indicate_size(&frame_in);
	sdo_set_indicated_size(&frame_in, 5);

	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	ASSERT_INT_GE(0, sdo_srv_dl_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_INIT_RES, sdo_get_cs(&frame_out));

	sdo_clear_frame(&frame_in);
	strcpy((char*)&frame_in.data[SDO_SEGMENT_IDX], "asdf");
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_set_segment_size(&frame_in, 5);
	sdo_end_segment(&frame_in);
	ASSERT_INT_GE(0, sdo_srv_dl_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_STR_EQ("asdf", _test_data);

	return 0;
}

int sdo_srv_dl_segmented_7bytes()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	memset(_test_data, -1, TEST_DATA_MAX_SIZE);
	_test_size = 7;

	sdo_indicate_size(&frame_in);
	sdo_set_indicated_size(&frame_in, 7);

	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	ASSERT_INT_GE(0, sdo_srv_dl_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_INIT_RES, sdo_get_cs(&frame_out));

	sdo_clear_frame(&frame_in);
	strcpy((char*)&frame_in.data[SDO_SEGMENT_IDX], "123456");
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_set_segment_size(&frame_in, 7);
	sdo_end_segment(&frame_in);
	ASSERT_INT_GE(0, sdo_srv_dl_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_STR_EQ("123456", _test_data);

	return 0;
}

int sdo_srv_dl_segmented_8bytes()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	memset(_test_data, -1, TEST_DATA_MAX_SIZE);
	_test_size = 8;

	sdo_indicate_size(&frame_in);
	sdo_set_indicated_size(&frame_in, 8);

	sdo_set_cs(&frame_in, SDO_CCS_DL_INIT_REQ);
	ASSERT_INT_GE(0, sdo_srv_dl_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_INIT_RES, sdo_get_cs(&frame_out));

	sdo_clear_frame(&frame_in);
	strcpy((char*)&frame_in.data[SDO_SEGMENT_IDX], "123456");
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_set_segment_size(&frame_in, 7);
	ASSERT_INT_GE(0, sdo_srv_dl_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DL_SEG_TOGGLED, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));

	frame_in.data[SDO_SEGMENT_IDX] = 42;
	sdo_set_cs(&frame_in, SDO_CCS_DL_SEG_REQ);
	sdo_set_segment_size(&frame_in, 1);
	sdo_end_segment(&frame_in);
	sdo_toggle(&frame_in);
	ASSERT_INT_GE(0, sdo_srv_dl_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_DL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_STR_EQ("123456", _test_data);
	ASSERT_INT_EQ(42, _test_data[7]);

	return 0;
}

int sdo_srv_ul_init_ok()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	strcpy((char*)&frame_in.data[SDO_MULTIPLEXER_IDX], "ab");
	ASSERT_INT_EQ(SDO_SRV_UL_SEG,
		      sdo_srv_ul_sm_init(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_UL_SEG, sm.state);
	ASSERT_INT_EQ(SDO_SCS_UL_INIT_RES, sdo_get_cs(&frame_out));
	ASSERT_STR_EQ("ab", (char*)&frame_out.data[SDO_MULTIPLEXER_IDX]);

	return 0;
}

int sdo_srv_ul_init_failed_cs()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_ABORT,
		      sdo_srv_ul_sm_init(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_ABORT, sm.state);
	ASSERT_INT_EQ(SDO_SCS_ABORT, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_ABORT_INVALID_CS, sdo_get_abort_code(&frame_out));

	return 0;
}

int sdo_srv_ul_init_remote_abort()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_ABORT);
	ASSERT_INT_EQ(SDO_SRV_INIT,
		      sdo_srv_ul_sm_init(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_INIT, sm.state);

	return 0;
}

int sdo_srv_ul_seg_ok()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_EQ(SDO_SRV_UL_SEG_TOGGLED,
		      sdo_srv_ul_sm_seg(&sm, &frame_in, &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_UL_SEG_TOGGLED, sm.state);
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_FALSE(sdo_is_toggled(&frame_out));

	return 0;
}

int sdo_srv_ul_seg_toggled_ok()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	sdo_toggle(&frame_in);
	ASSERT_INT_EQ(SDO_SRV_UL_SEG,
		      sdo_srv_ul_sm_seg(&sm, &frame_in, &frame_out, 1));
	ASSERT_INT_EQ(SDO_SRV_UL_SEG, sm.state);
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_TRUE(sdo_is_toggled(&frame_out));

	return 0;
}

int sdo_srv_ul_seg_failed_cs()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	ASSERT_INT_EQ(SDO_SRV_ABORT,
		      sdo_srv_ul_sm_seg(&sm, &frame_in, &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_ABORT, sm.state);
	ASSERT_INT_EQ(SDO_SCS_ABORT, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_ABORT_INVALID_CS, sdo_get_abort_code(&frame_out));

	return 0;
}

int sdo_srv_ul_seg_remote_abort()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	sdo_set_cs(&frame_in, SDO_CCS_ABORT);
	ASSERT_INT_EQ(SDO_SRV_REMOTE_ABORT,
		      sdo_srv_ul_sm_seg(&sm, &frame_in, &frame_out, 0));
	ASSERT_INT_EQ(SDO_SRV_REMOTE_ABORT, sm.state);

	return 0;
}

int sdo_srv_ul_expediated_1byte()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	_test_data[0] = 42;
	_test_size = 1;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	sdo_set_index(&frame_in, 1337);
	sdo_set_subindex(&frame_in, 1);

	ASSERT_INT_GE(0, sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);

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
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	strcpy(_test_data, "foo");
	_test_size = 4;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	sdo_set_index(&frame_in, 1234);
	sdo_set_subindex(&frame_in, 2);

	ASSERT_INT_GE(0, sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);

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
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	strcpy(_test_data, "abcd");
	_test_size = 5;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);
	sdo_set_index(&frame_in, 1000);
	sdo_set_subindex(&frame_in, 3);

	ASSERT_INT_GE(0, sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_INIT_RES, sdo_get_cs(&frame_out));

	ASSERT_INT_EQ(1000, _test_index);
	ASSERT_INT_EQ(3, _test_subindex);

	ASSERT_FALSE(sdo_is_expediated(&frame_out));
	ASSERT_TRUE(sdo_is_size_indicated(&frame_out));
	ASSERT_INT_EQ(5, sdo_get_indicated_size(&frame_out));

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_GE(0, sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_TRUE(sdo_is_end_segment(&frame_out));
	ASSERT_INT_EQ(5, sdo_get_segment_size(&frame_out));
	ASSERT_STR_EQ("abcd", (char*)&frame_out.data[SDO_SEGMENT_IDX]);

	return 0;
}

int sdo_srv_ul_segmented_7bytes()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	strcpy(_test_data, "123456");
	_test_size = 7;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);

	ASSERT_INT_GE(0, sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_INIT_RES, sdo_get_cs(&frame_out));

	ASSERT_FALSE(sdo_is_expediated(&frame_out));
	ASSERT_TRUE(sdo_is_size_indicated(&frame_out));
	ASSERT_INT_EQ(7, sdo_get_indicated_size(&frame_out));

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_GE(0, sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_TRUE(sdo_is_end_segment(&frame_out));
	ASSERT_INT_EQ(7, sdo_get_segment_size(&frame_out));
	ASSERT_STR_EQ("123456", (char*)&frame_out.data[SDO_SEGMENT_IDX]);

	return 0;
}

int sdo_srv_ul_segmented_8bytes()
{
	struct sdo_srv_sm sm = { .state = SDO_SRV_START };
	struct can_frame frame_in = { 0 }, frame_out = { 0 };

	memcpy(_test_data, "123456\08", 8);
	_test_size = 8;

	sdo_set_cs(&frame_in, SDO_CCS_UL_INIT_REQ);

	ASSERT_INT_GE(0, sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_INIT_RES, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_SRV_UL_SEG, sm.state);

	ASSERT_FALSE(sdo_is_expediated(&frame_out));

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	ASSERT_INT_GE(0, sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));
	ASSERT_INT_EQ(SDO_SRV_UL_SEG_TOGGLED, sm.state);

	ASSERT_FALSE(sdo_is_end_segment(&frame_out));
	ASSERT_INT_EQ(7, sdo_get_segment_size(&frame_out));
	ASSERT_STR_EQ("123456", (char*)&frame_out.data[SDO_SEGMENT_IDX]);

	sdo_set_cs(&frame_in, SDO_CCS_UL_SEG_REQ);
	sdo_toggle(&frame_in);
	ASSERT_INT_GE(0, sdo_srv_ul_sm_feed(&sm, &frame_in, &frame_out));
	ASSERT_INT_EQ(SDO_SRV_DONE, sm.state);
	ASSERT_INT_EQ(SDO_SCS_UL_SEG_RES, sdo_get_cs(&frame_out));

	ASSERT_TRUE(sdo_is_end_segment(&frame_out));
	ASSERT_INT_EQ(1, sdo_get_segment_size(&frame_out));
	ASSERT_INT_EQ('8', frame_out.data[SDO_SEGMENT_IDX]);

	return 0;
}

int sdo_match_obj_size_default()
{
	struct sdo_obj obj;
	enum sdo_abort_code code = 0xff;

	obj.flags = 0;
	obj.size = 42;
	
	ASSERT_TRUE(sdo_match_obj_size(&obj, 42, &code));
	ASSERT_INT_EQ(0, code);

	ASSERT_FALSE(sdo_match_obj_size(&obj, 41, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_SHORT, code);

	ASSERT_FALSE(sdo_match_obj_size(&obj, 43, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_LONG, code);

	return 0;
}

int sdo_match_obj_size_eq()
{
	struct sdo_obj obj;
	enum sdo_abort_code code = 0xff;

	obj.flags = SDO_OBJ_EQ;
	obj.size = 42;
	
	ASSERT_TRUE(sdo_match_obj_size(&obj, 42, &code));
	ASSERT_INT_EQ(0, code);

	ASSERT_FALSE(sdo_match_obj_size(&obj, 41, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_SHORT, code);

	ASSERT_FALSE(sdo_match_obj_size(&obj, 43, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_LONG, code);

	return 0;
}

int sdo_match_obj_size_lt()
{
	struct sdo_obj obj;
	enum sdo_abort_code code = 0xff;

	obj.flags = SDO_OBJ_LT;
	obj.size = 42;
	
	ASSERT_FALSE(sdo_match_obj_size(&obj, 42, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_LONG, code);

	ASSERT_TRUE(sdo_match_obj_size(&obj, 41, &code));
	ASSERT_INT_EQ(0, code);

	ASSERT_FALSE(sdo_match_obj_size(&obj, 43, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_LONG, code);

	return 0;
}

int sdo_match_obj_size_le()
{
	struct sdo_obj obj;
	enum sdo_abort_code code = 0xff;

	obj.flags = SDO_OBJ_LE;
	obj.size = 42;
	
	ASSERT_TRUE(sdo_match_obj_size(&obj, 42, &code));
	ASSERT_INT_EQ(0, code);

	ASSERT_TRUE(sdo_match_obj_size(&obj, 41, &code));
	ASSERT_INT_EQ(0, code);

	ASSERT_FALSE(sdo_match_obj_size(&obj, 43, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_LONG, code);

	return 0;
}

int sdo_match_obj_size_gt()
{
	struct sdo_obj obj;
	enum sdo_abort_code code = 0xff;

	obj.flags = SDO_OBJ_GT;
	obj.size = 42;
	
	ASSERT_FALSE(sdo_match_obj_size(&obj, 42, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_SHORT, code);

	ASSERT_FALSE(sdo_match_obj_size(&obj, 41, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_SHORT, code);

	ASSERT_TRUE(sdo_match_obj_size(&obj, 43, &code));
	ASSERT_INT_EQ(0, code);

	return 0;
}

int sdo_match_obj_size_ge()
{
	struct sdo_obj obj;
	enum sdo_abort_code code = 0xff;

	obj.flags = SDO_OBJ_GE;
	obj.size = 42;
	
	ASSERT_TRUE(sdo_match_obj_size(&obj, 42, &code));
	ASSERT_INT_EQ(0, code);

	ASSERT_FALSE(sdo_match_obj_size(&obj, 41, &code));
	ASSERT_INT_EQ(SDO_ABORT_TOO_SHORT, code);

	ASSERT_TRUE(sdo_match_obj_size(&obj, 43, &code));
	ASSERT_INT_EQ(0, code);

	return 0;
}

int main()
{
	int r = 0;
	sdo_get_obj = NULL;

	fprintf(stderr, "Misc:\n");
	RUN_TEST(set_get_cs);
	RUN_TEST(set_get_segment_size);
	RUN_TEST(set_get_abort_code);

	fprintf(stderr, "\nsdo_match_obj_size:\n");
	RUN_TEST(sdo_match_obj_size_default);
	RUN_TEST(sdo_match_obj_size_eq);
	RUN_TEST(sdo_match_obj_size_lt);
	RUN_TEST(sdo_match_obj_size_le);
	RUN_TEST(sdo_match_obj_size_gt);
	RUN_TEST(sdo_match_obj_size_ge);

	fprintf(stderr, "\nServer download state machine:\n");
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

	fprintf(stderr, "\nServer download:\n");
	sdo_get_obj = my_sdo_get_obj;
	RUN_TEST(sdo_srv_dl_expediated_1byte);
	RUN_TEST(sdo_srv_dl_expediated_4bytes);
	RUN_TEST(sdo_srv_dl_segmented_5bytes);
	RUN_TEST(sdo_srv_dl_segmented_7bytes);
	RUN_TEST(sdo_srv_dl_segmented_8bytes);

	fprintf(stderr, "\nServer upload state machine:\n");
	sdo_get_obj = NULL;
	RUN_TEST(sdo_srv_ul_init_ok);
	RUN_TEST(sdo_srv_ul_init_failed_cs);
	RUN_TEST(sdo_srv_ul_init_remote_abort);
	RUN_TEST(sdo_srv_ul_seg_ok);
	RUN_TEST(sdo_srv_ul_seg_toggled_ok);
	RUN_TEST(sdo_srv_ul_seg_failed_cs);
	RUN_TEST(sdo_srv_ul_seg_remote_abort);

	fprintf(stderr, "\nServer upload:\n");
	sdo_get_obj = my_sdo_get_obj;
	RUN_TEST(sdo_srv_ul_expediated_1byte);
	RUN_TEST(sdo_srv_ul_expediated_4bytes);
	RUN_TEST(sdo_srv_ul_segmented_5bytes);
	RUN_TEST(sdo_srv_ul_segmented_7bytes);
	RUN_TEST(sdo_srv_ul_segmented_8bytes);

	return r;
}

