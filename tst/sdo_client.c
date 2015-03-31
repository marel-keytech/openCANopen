#include <linux/can.h>

#include "tst.h"
#include "canopen/sdo.h"
#include "canopen/sdo_client.h"
#include "canopen/sdo_client_priv.h"

static int test_make_dl_request_1_byte()
{
	struct sdo_dl_req req;
	memset(&req, 0, sizeof(req));
	sdo_request_download(&req, 0x1000, 42, "x", 1);

	ASSERT_INT_EQ(SDO_REQ_INIT_EXPEDIATED, req.state);
	ASSERT_INT_EQ(SDO_CCS_DL_INIT_REQ, sdo_get_cs(&req.frame));
	ASSERT_INT_EQ(1, sdo_get_expediated_size(&req.frame));
	ASSERT_INT_EQ(5, req.frame.can_dlc);
	ASSERT_TRUE(sdo_is_expediated(&req.frame));
	ASSERT_TRUE(sdo_is_size_indicated(&req.frame));
	ASSERT_INT_EQ(0x1000, sdo_get_index(&req.frame));
	ASSERT_INT_EQ(42, sdo_get_subindex(&req.frame));
	ASSERT_INT_EQ('x', req.frame.data[SDO_EXPEDIATED_DATA_IDX]);
	ASSERT_TRUE(req.have_frame);

	return 0;
}

static int test_make_dl_request_4_bytes()
{
	struct sdo_dl_req req;
	memset(&req, 0, sizeof(req));
	sdo_request_download(&req, 0x1000, 42, "foo", 4);

	ASSERT_INT_EQ(SDO_REQ_INIT_EXPEDIATED, req.state);
	ASSERT_INT_EQ(SDO_CCS_DL_INIT_REQ, sdo_get_cs(&req.frame));
	ASSERT_INT_EQ(4, sdo_get_expediated_size(&req.frame));
	ASSERT_INT_EQ(8, req.frame.can_dlc);
	ASSERT_TRUE(sdo_is_expediated(&req.frame));
	ASSERT_TRUE(sdo_is_size_indicated(&req.frame));
	ASSERT_INT_EQ(0x1000, sdo_get_index(&req.frame));
	ASSERT_INT_EQ(42, sdo_get_subindex(&req.frame));
	ASSERT_STR_EQ("foo", (char*)&req.frame.data[SDO_EXPEDIATED_DATA_IDX]);
	ASSERT_TRUE(req.have_frame);

	return 0;
}

static int test_make_dl_request_5_bytes()
{
	struct sdo_dl_req req;
	memset(&req, 0, sizeof(req));
	sdo_request_download(&req, 0x1000, 42, (void*)0xdeadbeef, 5);

	ASSERT_INT_EQ(SDO_REQ_INIT, req.state);
	ASSERT_INT_EQ(5, req.size);
	ASSERT_INT_EQ(0, req.pos);
	ASSERT_PTR_EQ((void*)0xdeadbeef, (void*)req.addr);
	ASSERT_INT_EQ(SDO_CCS_DL_INIT_REQ, sdo_get_cs(&req.frame));
	ASSERT_INT_EQ(5, sdo_get_indicated_size(&req.frame));
	ASSERT_INT_EQ(8, req.frame.can_dlc);
	ASSERT_FALSE(sdo_is_expediated(&req.frame));
	ASSERT_TRUE(sdo_is_size_indicated(&req.frame));
	ASSERT_INT_EQ(0x1000, sdo_get_index(&req.frame));
	ASSERT_INT_EQ(42, sdo_get_subindex(&req.frame));
	ASSERT_TRUE(req.have_frame);

	return 0;
}

static int test_expediated_download_success()
{
	struct sdo_dl_req req;
	struct can_frame rcf;

	sdo_clear_frame(&rcf);

	ASSERT_TRUE(sdo_request_download(&req, 0x1000, 42, "foo", 4));
	ASSERT_INT_EQ(8, req.frame.can_dlc);

	sdo_set_cs(&rcf, SDO_SCS_DL_INIT_RES);
	sdo_set_index(&rcf, 0x1000);
	sdo_set_subindex(&rcf, 42);

	ASSERT_INT_EQ(0, sdo_dl_req_feed(&req, &rcf));
	ASSERT_INT_EQ(SDO_REQ_DONE, req.state);

	return 0;
}

static int test_expediated_download_abort()
{
	struct sdo_dl_req req;
	struct can_frame rcf;

	sdo_clear_frame(&rcf);

	ASSERT_TRUE(sdo_request_download(&req, 0x1000, 42, "foo", 4));
	ASSERT_INT_EQ(8, req.frame.can_dlc);

	sdo_abort(&rcf, SDO_ABORT_NEXIST, 0x1000, 42);

	ASSERT_INT_EQ(0, sdo_dl_req_feed(&req, &rcf));
	ASSERT_INT_EQ(SDO_REQ_REMOTE_ABORT, req.state);

	return 0;
}

static int test_segmented_download_success_one_segment()
{
	struct sdo_dl_req req;
	struct can_frame rcf;

	sdo_clear_frame(&rcf);

	ASSERT_TRUE(sdo_request_download(&req, 0x1000, 42, "foobar", 7));
	ASSERT_INT_EQ(8, req.frame.can_dlc);

	sdo_set_cs(&rcf, SDO_SCS_DL_INIT_RES);
	sdo_set_index(&rcf, 0x1000);
	sdo_set_subindex(&rcf, 42);

	ASSERT_INT_EQ(1, sdo_dl_req_feed(&req, &rcf));
	ASSERT_INT_EQ(8, req.frame.can_dlc);

	ASSERT_INT_EQ(SDO_CCS_DL_SEG_REQ, sdo_get_cs(&req.frame));
	ASSERT_INT_EQ(7, sdo_get_segment_size(&req.frame));
	ASSERT_TRUE(sdo_is_end_segment(&req.frame));
	ASSERT_FALSE(sdo_is_toggled(&req.frame));
	ASSERT_STR_EQ("foobar", (char*)&req.frame.data[SDO_SEGMENT_IDX]);

	sdo_set_cs(&rcf, SDO_SCS_DL_SEG_RES);

	ASSERT_INT_EQ(0, sdo_dl_req_feed(&req, &rcf));
	ASSERT_INT_EQ(SDO_REQ_DONE, req.state);

	return 0;
}

static int test_segmented_download_success_two_segments()
{
	struct sdo_dl_req req;
	struct can_frame rcf;

	sdo_clear_frame(&rcf);

	ASSERT_TRUE(sdo_request_download(&req, 0x1000, 42, "foobar\0x", 8));

	sdo_set_cs(&rcf, SDO_SCS_DL_INIT_RES);
	sdo_set_index(&rcf, 0x1000);
	sdo_set_subindex(&rcf, 42);

	ASSERT_INT_EQ(1, sdo_dl_req_feed(&req, &rcf));
	ASSERT_INT_EQ(8, req.frame.can_dlc);

	ASSERT_INT_EQ(SDO_CCS_DL_SEG_REQ, sdo_get_cs(&req.frame));
	ASSERT_INT_EQ(7, sdo_get_segment_size(&req.frame));
	ASSERT_FALSE(sdo_is_end_segment(&req.frame));
	ASSERT_FALSE(sdo_is_toggled(&req.frame));
	ASSERT_STR_EQ("foobar", (char*)&req.frame.data[SDO_SEGMENT_IDX]);

	sdo_set_cs(&rcf, SDO_SCS_DL_SEG_RES);

	ASSERT_INT_EQ(1, sdo_dl_req_feed(&req, &rcf));
	ASSERT_INT_EQ(2, req.frame.can_dlc);

	ASSERT_INT_EQ(SDO_CCS_DL_SEG_REQ, sdo_get_cs(&req.frame));
	ASSERT_INT_EQ(1, sdo_get_segment_size(&req.frame));
	ASSERT_TRUE(sdo_is_end_segment(&req.frame));
	ASSERT_FALSE(sdo_is_toggled(&req.frame));
	ASSERT_INT_EQ('x', req.frame.data[SDO_SEGMENT_IDX]);

	sdo_toggle(&rcf);

	ASSERT_INT_EQ(0, sdo_dl_req_feed(&req, &rcf));
	ASSERT_INT_EQ(SDO_REQ_DONE, req.state);

	return 0;
}

static int test_segmented_download_abort_one_segment()
{
	struct sdo_dl_req req;
	struct can_frame rcf;

	sdo_clear_frame(&rcf);

	ASSERT_TRUE(sdo_request_download(&req, 0x1000, 42, "foobar", 7));

	sdo_set_cs(&rcf, SDO_SCS_DL_INIT_RES);
	sdo_set_index(&rcf, 0x1000);
	sdo_set_subindex(&rcf, 42);

	ASSERT_INT_EQ(1, sdo_dl_req_feed(&req, &rcf));
	ASSERT_INT_EQ(8, req.frame.can_dlc);

	ASSERT_INT_EQ(SDO_CCS_DL_SEG_REQ, sdo_get_cs(&req.frame));
	ASSERT_INT_EQ(7, sdo_get_segment_size(&req.frame));
	ASSERT_TRUE(sdo_is_end_segment(&req.frame));
	ASSERT_FALSE(sdo_is_toggled(&req.frame));
	ASSERT_STR_EQ("foobar", (char*)&req.frame.data[SDO_SEGMENT_IDX]);

	sdo_set_cs(&rcf, SDO_SCS_ABORT);

	ASSERT_INT_EQ(0, sdo_dl_req_feed(&req, &rcf));
	ASSERT_INT_EQ(SDO_REQ_REMOTE_ABORT, req.state);

	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_make_dl_request_1_byte);
	RUN_TEST(test_make_dl_request_4_bytes);
	RUN_TEST(test_make_dl_request_5_bytes);

	RUN_TEST(test_expediated_download_success);
	RUN_TEST(test_expediated_download_abort);

	RUN_TEST(test_segmented_download_success_one_segment);
	RUN_TEST(test_segmented_download_success_two_segments);
	RUN_TEST(test_segmented_download_abort_one_segment);

	return 0;
}

