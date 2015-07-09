#ifndef TESTING
#define TESTING
#endif

#include <linux/can.h>

#include "tst.h"
#include "canopen/sdo.h"
#include "canopen/sdo_client.h"
#include "canopen.h"

static int test_make_dl_request_1_byte()
{
	struct sdo_req req;
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
	struct sdo_req req;
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
	struct sdo_req req;
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
	struct sdo_req req;
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
	struct sdo_req req;
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
	struct sdo_req req;
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
	struct sdo_req req;
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
	struct sdo_req req;
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

static int test_make_ul_request()
{
	struct sdo_req req;
	memset(&req, 0, sizeof(req));
	sdo_request_upload(&req, 0x1000, 42);

	ASSERT_INT_EQ(SDO_REQ_INIT, req.state);
	ASSERT_INT_EQ(SDO_CCS_UL_INIT_REQ, sdo_get_cs(&req.frame));
	ASSERT_INT_EQ(0x1000, sdo_get_index(&req.frame));
	ASSERT_INT_EQ(42, sdo_get_subindex(&req.frame));
	ASSERT_INT_EQ(4, req.frame.can_dlc);
	ASSERT_INT_EQ(-1, req.indicated_size);
	ASSERT_INT_EQ(0x1000, req.index);
	ASSERT_INT_EQ(42, req.subindex);
	ASSERT_TRUE(req.have_frame);

	return 0;
}

static int test_expediated_upload_1_byte_size_indicated()
{
	struct sdo_req req;
	struct can_frame rcf;

	sdo_clear_frame(&rcf);

	ASSERT_TRUE(sdo_request_upload(&req, 0x1000, 42));

	sdo_set_cs(&rcf, SDO_SCS_UL_INIT_RES);
	sdo_expediate(&rcf);
	sdo_indicate_size(&rcf);
	sdo_set_expediated_size(&rcf, 1);
	sdo_copy_multiplexer(&rcf, &req.frame);
	rcf.data[SDO_EXPEDIATED_DATA_IDX] = 23;

	int save = 0;
	req.addr = (void*)&save;
	req.size = 1;

	ASSERT_FALSE(sdo_ul_req_feed(&req, &rcf));

	ASSERT_INT_EQ(SDO_REQ_DONE, req.state);
	ASSERT_FALSE(req.have_frame);
	ASSERT_INT_EQ(23, save);
	ASSERT_INT_EQ(1, req.indicated_size);

	return 0;
}

static int test_expediated_upload_1_byte_size_not_indicated()
{
	struct sdo_req req;
	struct can_frame rcf;


	ASSERT_TRUE(sdo_request_upload(&req, 0x1000, 42));

	sdo_clear_frame(&rcf);
	sdo_set_cs(&rcf, SDO_SCS_UL_INIT_RES);
	sdo_expediate(&rcf);
	sdo_copy_multiplexer(&rcf, &req.frame);
	rcf.data[SDO_EXPEDIATED_DATA_IDX] = 23;

	int save = 0;
	req.addr = (void*)&save;
	req.size = 1;

	ASSERT_FALSE(sdo_ul_req_feed(&req, &rcf));

	ASSERT_INT_EQ(SDO_REQ_DONE, req.state);
	ASSERT_FALSE(req.have_frame);
	ASSERT_INT_EQ(23, save);
	ASSERT_INT_EQ(-1, req.indicated_size);

	return 0;
}

static int test_segmented_upload_7_bytes_size_indicated()
{
	struct sdo_req req;
	struct can_frame rcf;

	ASSERT_TRUE(sdo_request_upload(&req, 0x1000, 42));

	sdo_clear_frame(&rcf);
	sdo_set_cs(&rcf, SDO_SCS_UL_INIT_RES);
	sdo_indicate_size(&rcf);
	sdo_set_indicated_size(&rcf, 7);
	sdo_copy_multiplexer(&rcf, &req.frame);

	ASSERT_TRUE(sdo_ul_req_feed(&req, &rcf));

	ASSERT_INT_EQ(SDO_REQ_SEG, req.state);
	ASSERT_TRUE(req.have_frame);
	ASSERT_INT_EQ(SDO_CCS_UL_SEG_REQ, sdo_get_cs(&req.frame));
	ASSERT_FALSE(sdo_is_toggled(&req.frame));
	ASSERT_FALSE(req.is_toggled);
	ASSERT_INT_EQ(1, req.frame.can_dlc);
	ASSERT_INT_EQ(7, req.indicated_size);

	sdo_clear_frame(&rcf);
	sdo_set_cs(&rcf, SDO_SCS_UL_SEG_RES);
	sdo_set_segment_size(&rcf, 7);
	sdo_end_segment(&rcf);
	memcpy(&rcf.data[SDO_SEGMENT_IDX], "foobar", 7);

	char buffer[7];
	req.addr = buffer;
	req.size = 7;

	ASSERT_FALSE(sdo_ul_req_feed(&req, &rcf));

	ASSERT_INT_EQ(SDO_REQ_DONE, req.state);
	ASSERT_FALSE(req.have_frame);
	ASSERT_STR_EQ("foobar", buffer);
	ASSERT_INT_EQ(7, req.indicated_size);

	return 0;
}

struct mock_timer {
	int timeout;
	int started;
	int stopped;
	sdo_proc_fn timeout_fn;
};

static struct mock_timer mock_timer;

static void mock_start_timer(void* timer, int timeout)
{
	struct mock_timer* mock_timer = timer;
	mock_timer->timeout = timeout;
	mock_timer->started++;
}

static void mock_stop_timer(void* timer)
{
	struct mock_timer* mock_timer = timer;
	mock_timer->stopped++;
}

static void mock_set_timer_fn(void* timer, sdo_proc_fn fn)
{
	struct mock_timer* mock_timer = timer;
	mock_timer->timeout_fn = fn;
}

static struct can_frame mock_written_frame;

static ssize_t mock_write_frame(const struct can_frame* cf)
{
	mock_written_frame = *cf;
	return sizeof(mock_written_frame);
}

static void set_proc_callbacks(struct sdo_proc* proc)
{
	proc->async_timer = &mock_timer;
	proc->do_start_timer = mock_start_timer;
	proc->do_stop_timer = mock_stop_timer;
	proc->do_set_timer_fn = mock_set_timer_fn;
	proc->do_write_frame = mock_write_frame;
}

static void init_mock_proc(struct sdo_proc* proc)
{
	memset(&mock_timer, 0, sizeof(mock_timer));
	memset(proc, 0, sizeof(*proc));
	set_proc_callbacks(proc);
	sdo_proc_init(proc);
}

static int test_proc_init_destroy()
{
	struct sdo_proc proc;
	memset(&proc, 0, sizeof(proc));
	set_proc_callbacks(&proc);
	ASSERT_INT_EQ(0, sdo_proc_init(&proc));
	ASSERT_PTR_EQ(sdo_proc__async_timeout, mock_timer.timeout_fn);
	sdo_proc_destroy(&proc);
	return 0;
}

static int test_proc_setup_dl_req()
{
	struct sdo_proc proc;
	init_mock_proc(&proc);

	char buffer[sizeof(struct sdo_proc__req) + 32];
	struct sdo_proc__req* rdata = (struct sdo_proc__req*)buffer;
	proc.current_req_data = rdata;

	proc.nodeid = 11;
	rdata->index = 0x1234;
	rdata->subindex = 42;
	strcpy(rdata->data, "foobar");
	rdata->size = strlen("foobar") + 1;

	memset(&mock_written_frame, -1, sizeof(mock_written_frame));

	sdo_proc__setup_dl_req(&proc);

	ASSERT_INT_EQ(0x1234, sdo_get_index(&proc.req.frame));
	ASSERT_INT_EQ(42, sdo_get_subindex(&proc.req.frame));
	ASSERT_INT_EQ(SDO_CCS_DL_INIT_REQ, sdo_get_cs(&proc.req.frame));
	ASSERT_FALSE(sdo_is_expediated(&proc.req.frame));

	ASSERT_STR_EQ("foobar", proc.req.addr);
	ASSERT_INT_EQ(strlen("foobar") + 1, proc.req.size);

	proc.current_req_data = NULL;
	sdo_proc_destroy(&proc);
	return 0;
}

static int test_proc_setup_ul_req()
{
	struct sdo_proc proc;
	init_mock_proc(&proc);

	char buffer[sizeof(struct sdo_proc__req) + 32];
	struct sdo_proc__req* rdata = (struct sdo_proc__req*)buffer;
	proc.current_req_data = rdata;

	proc.nodeid = 11;
	rdata->index = 0x1234;
	rdata->subindex = 42;
	rdata->size = 32;

	memset(&mock_written_frame, -1, sizeof(mock_written_frame));

	sdo_proc__setup_ul_req(&proc);

	ASSERT_INT_EQ(0x1234, sdo_get_index(&proc.req.frame));
	ASSERT_INT_EQ(42, sdo_get_subindex(&proc.req.frame));
	ASSERT_INT_EQ(SDO_CCS_UL_INIT_REQ, sdo_get_cs(&proc.req.frame));

	ASSERT_INT_EQ(0, proc.req.pos);
	ASSERT_PTR_EQ(rdata->data, proc.req.addr);
	ASSERT_INT_EQ(32, proc.req.size);

	proc.current_req_data = NULL;
	sdo_proc_destroy(&proc);
	return 0;
}

static int test_proc_try_next_request__empty()
{
	struct sdo_proc proc;
	init_mock_proc(&proc);

	memset(&mock_written_frame, 0, sizeof(mock_written_frame));

	sdo_proc__try_next_request(&proc);

	ASSERT_INT_EQ(0, mock_written_frame.can_id);
	ASSERT_PTR_EQ(NULL, proc.current_req_data);

	proc.current_req_data = NULL;
	sdo_proc_destroy(&proc);
	return 0;
}

static int test_proc_try_next_request__one_sdo()
{
	struct sdo_proc proc;
	init_mock_proc(&proc);

	char buffer[sizeof(struct sdo_proc__req) + 32];
	struct sdo_proc__req* rdata = (struct sdo_proc__req*)buffer;

	proc.nodeid = 11;
	rdata->type = SDO_REQ_UL;
	rdata->index = 0x1234;
	rdata->subindex = 42;
	rdata->size = 32;

	memset(&mock_written_frame, -1, sizeof(mock_written_frame));

	ptr_fifo_enqueue(&proc.sdo_req_input, rdata, NULL);

	sdo_proc__try_next_request(&proc);

	ASSERT_INT_EQ(R_RSDO + 11, mock_written_frame.can_id);
	ASSERT_INT_EQ(SDO_CCS_UL_INIT_REQ, sdo_get_cs(&mock_written_frame));
	ASSERT_PTR_EQ(rdata, proc.current_req_data);

	proc.current_req_data = NULL;
	sdo_proc_destroy(&proc);
	return 0;
}

struct mock_sdo_proc {
	struct sdo_proc proc;
	int on_done_called;
};

static int test_proc_is_req_done()
{
	struct sdo_proc proc;
	init_mock_proc(&proc);

	proc.req.state = SDO_REQ_INIT;
	ASSERT_FALSE(sdo_proc__is_req_done(&proc));
	proc.req.state = SDO_REQ_DONE;
	ASSERT_TRUE(sdo_proc__is_req_done(&proc));
	proc.req.state = SDO_REQ_ABORTED;
	ASSERT_TRUE(sdo_proc__is_req_done(&proc));

	sdo_proc_destroy(&proc);
	return 0;
}

static int test_proc_start_timer()
{
	struct sdo_proc proc;
	init_mock_proc(&proc);

	struct sdo_proc__req rdata;
	rdata.timeout = 1337;
	proc.current_req_data = &rdata;

	sdo_proc__start_timer(&proc);

	ASSERT_INT_EQ(1, mock_timer.started);
	ASSERT_INT_EQ(1337, mock_timer.timeout);

	proc.current_req_data = NULL;
	sdo_proc_destroy(&proc);
	return 0;
}

static int test_proc_stop_timer()
{
	struct sdo_proc proc;
	init_mock_proc(&proc);

	sdo_proc__stop_timer(&proc);

	ASSERT_INT_EQ(1, mock_timer.stopped);

	sdo_proc_destroy(&proc);
	return 0;
}

static int test_proc_restart_timer()
{
	struct sdo_proc proc;
	init_mock_proc(&proc);

	struct sdo_proc__req rdata;
	rdata.timeout = 1337;
	proc.current_req_data = &rdata;

	sdo_proc__restart_timer(&proc);

	ASSERT_INT_EQ(1, mock_timer.stopped);
	ASSERT_INT_EQ(1, mock_timer.started);
	ASSERT_INT_EQ(1337, mock_timer.timeout);

	proc.current_req_data = NULL;
	sdo_proc_destroy(&proc);
	return 0;
}


int main()
{
	int r = 0;

	printf("sdo_client:\n");
	RUN_TEST(test_make_dl_request_1_byte);
	RUN_TEST(test_make_dl_request_4_bytes);
	RUN_TEST(test_make_dl_request_5_bytes);

	RUN_TEST(test_expediated_download_success);
	RUN_TEST(test_expediated_download_abort);

	RUN_TEST(test_segmented_download_success_one_segment);
	RUN_TEST(test_segmented_download_success_two_segments);
	RUN_TEST(test_segmented_download_abort_one_segment);

	RUN_TEST(test_make_ul_request);
	RUN_TEST(test_expediated_upload_1_byte_size_indicated);
	RUN_TEST(test_expediated_upload_1_byte_size_not_indicated);

	RUN_TEST(test_segmented_upload_7_bytes_size_indicated);

	printf("sdo_proc:\n");
	RUN_TEST(test_proc_init_destroy);
	RUN_TEST(test_proc_setup_dl_req);
	RUN_TEST(test_proc_setup_ul_req);
	RUN_TEST(test_proc_try_next_request__empty);
	RUN_TEST(test_proc_try_next_request__one_sdo);
	RUN_TEST(test_proc_is_req_done);
	RUN_TEST(test_proc_start_timer);
	RUN_TEST(test_proc_stop_timer);
	RUN_TEST(test_proc_restart_timer);

	return r;
}

