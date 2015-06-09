#include <poll.h>
#include <unistd.h>
#include <time.h>

#include "tst.h"
#include "fff.h"
#include "canopen/network.h"
#include "canopen/nmt.h"
#include "canopen/sdo.h"
#include "canopen/heartbeat.h"
#include "socketcan.h"
#include "canopen.h"

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, poll, struct pollfd*, nfds_t, int);
FAKE_VALUE_FUNC(ssize_t, read, int, void*, size_t);
FAKE_VALUE_FUNC(ssize_t, write, int, const void*, size_t);
FAKE_VALUE_FUNC(int, clock_gettime, clockid_t, struct timespec*);

int test_net_write()
{
	RESET_FAKE(poll);
	RESET_FAKE(write);

	poll_fake.return_val = 1;
	write_fake.return_val = 5;

	ASSERT_INT_EQ(5, net_write(42, (const void*)0xdeadbeef, 5, 1337));

	ASSERT_INT_EQ(42, write_fake.arg0_val);
	ASSERT_PTR_EQ((void*)0xdeadbeef, (void*)write_fake.arg1_val);
	ASSERT_INT_EQ(5, write_fake.arg2_val);

	ASSERT_INT_EQ(1, poll_fake.arg1_val);
	ASSERT_INT_EQ(1337, poll_fake.arg2_val);

	return 0;
}

int test_net_read()
{
	RESET_FAKE(poll);
	RESET_FAKE(read);

	poll_fake.return_val = 1;
	read_fake.return_val = 5;

	ASSERT_INT_EQ(5, net_read(42, (void*)0xdeadbeef, 5, 1337));

	ASSERT_INT_EQ(42, read_fake.arg0_val);
	ASSERT_PTR_EQ((void*)0xdeadbeef, read_fake.arg1_val);
	ASSERT_INT_EQ(5, read_fake.arg2_val);

	ASSERT_INT_EQ(1, poll_fake.arg1_val);
	ASSERT_INT_EQ(1337, poll_fake.arg2_val);

	return 0;
}

struct can_frame saved_can_frame_;

ssize_t save_can_frame(int fd, const void* addr, size_t size)
{
	(void)fd;
	(void)size;

	memcpy(&saved_can_frame_, addr, sizeof(saved_can_frame_));

	return sizeof(saved_can_frame_);
}

int test_net__send_nmt()
{
	RESET_FAKE(poll);
	RESET_FAKE(write);
	memset(&saved_can_frame_, 0, sizeof(saved_can_frame_));

	write_fake.custom_fake = save_can_frame;

	poll_fake.return_val = 1;
	write_fake.return_val = sizeof(struct can_frame);

	ASSERT_INT_GE(0, net__send_nmt(42, 7, 11));

	ASSERT_INT_EQ(42, write_fake.arg0_val);
	ASSERT_INT_EQ(sizeof(struct can_frame), write_fake.arg2_val);

	ASSERT_INT_EQ(0, saved_can_frame_.can_id);
	ASSERT_INT_EQ(2, saved_can_frame_.can_dlc);

	ASSERT_INT_EQ(7, nmt_get_cs(&saved_can_frame_));
	ASSERT_INT_EQ(11, nmt_get_nodeid(&saved_can_frame_));

	return 0;
}

int test_net__request_device_type()
{
	RESET_FAKE(poll);
	RESET_FAKE(write);
	memset(&saved_can_frame_, 0, sizeof(saved_can_frame_));

	write_fake.custom_fake = save_can_frame;

	poll_fake.return_val = 1;
	write_fake.return_val = sizeof(struct can_frame);

	ASSERT_INT_GE(0, net__request_device_type(42, 10));

	ASSERT_INT_EQ(42, write_fake.arg0_val);
	ASSERT_INT_EQ(sizeof(struct can_frame), write_fake.arg2_val);

	ASSERT_INT_EQ(R_RSDO + 10, saved_can_frame_.can_id);
	ASSERT_INT_EQ(4, saved_can_frame_.can_dlc);

	ASSERT_INT_EQ(SDO_CCS_UL_INIT_REQ, sdo_get_cs(&saved_can_frame_));
	ASSERT_INT_EQ(0x1000, sdo_get_index(&saved_can_frame_));
	ASSERT_INT_EQ(0, sdo_get_subindex(&saved_can_frame_));

	return 0;
}

static struct timespec gettime_timespec;

int custom_gettime(clockid_t id, struct timespec* ts)
{
	(void)id;
	*ts = gettime_timespec;
	return 0;
}

int test_net__gettime_ms()
{
	RESET_FAKE(clock_gettime);
	clock_gettime_fake.custom_fake = custom_gettime;

	gettime_timespec.tv_sec = 1;
	gettime_timespec.tv_nsec = 1000000;

	ASSERT_INT_EQ(1001, net__gettime_ms());

	return 0;
}

static char enabled_nodes[128];
int enabled_nodes_index;

static ssize_t read_bootup(int fd, void* dst, size_t size)
{
	(void)fd;
	struct can_frame* cf = dst;

	while (enabled_nodes_index < 128 && !enabled_nodes[enabled_nodes_index])
		++enabled_nodes_index;

	if (enabled_nodes_index > 127)
		return -1;

	cf->can_id = R_HEARTBEAT + enabled_nodes_index;
	heartbeat_set_state(cf, NMT_STATE_BOOTUP);

	++enabled_nodes_index;

	return sizeof(struct can_frame);
}

int test_net__wait_for_bootup()
{
	RESET_FAKE(poll);
	RESET_FAKE(read);
	RESET_FAKE(clock_gettime);

	read_fake.custom_fake = read_bootup;
	poll_fake.return_val = 1;

	memset(enabled_nodes, 0, sizeof(enabled_nodes));
	enabled_nodes_index = 0;

	enabled_nodes[0] = 1;
	enabled_nodes[7] = 1;
	enabled_nodes[127] = 1;

	char nodes_seen[128];
	memset(nodes_seen, 0, sizeof(nodes_seen));
	ASSERT_INT_EQ(0, net__wait_for_bootup(42, nodes_seen, 0, 127, 1000));

	ASSERT_INT_EQ(4, poll_fake.call_count);
	ASSERT_INT_EQ(4, read_fake.call_count);
	ASSERT_INT_EQ(42, read_fake.arg0_val);

	ASSERT_TRUE(nodes_seen[0]);
	ASSERT_FALSE(nodes_seen[1]);
	ASSERT_FALSE(nodes_seen[2]);
	ASSERT_FALSE(nodes_seen[6]);
	ASSERT_TRUE(nodes_seen[7]);
	ASSERT_FALSE(nodes_seen[8]);
	ASSERT_FALSE(nodes_seen[126]);
	ASSERT_TRUE(nodes_seen[127]);

	return 0;
}

static ssize_t read_sdo(int fd, void* dst, size_t size)
{
	(void)fd;
	struct can_frame* cf = dst;

	while (enabled_nodes_index < 128 && !enabled_nodes[enabled_nodes_index])
		++enabled_nodes_index;

	if (enabled_nodes_index > 127)
		return -1;

	cf->can_id = R_TSDO + enabled_nodes_index;
	sdo_set_cs(cf, SDO_SCS_UL_INIT_RES);
	sdo_set_index(cf, 0x1000);
	sdo_set_subindex(cf, 0);

	++enabled_nodes_index;

	return sizeof(struct can_frame);
}

int test_net__wait_for_sdo()
{
	RESET_FAKE(poll);
	RESET_FAKE(read);
	RESET_FAKE(clock_gettime);

	read_fake.custom_fake = read_sdo;
	poll_fake.return_val = 1;

	memset(enabled_nodes, 0, sizeof(enabled_nodes));
	enabled_nodes_index = 0;

	enabled_nodes[0] = 1;
	enabled_nodes[7] = 1;
	enabled_nodes[127] = 1;

	char nodes_seen[128];
	memset(nodes_seen, 0, sizeof(nodes_seen));
	ASSERT_INT_EQ(0, net__wait_for_sdo(42, nodes_seen, 0, 127, 1000));

	ASSERT_INT_EQ(4, poll_fake.call_count);
	ASSERT_INT_EQ(4, read_fake.call_count);
	ASSERT_INT_EQ(42, read_fake.arg0_val);

	ASSERT_TRUE(nodes_seen[0]);
	ASSERT_FALSE(nodes_seen[1]);
	ASSERT_FALSE(nodes_seen[2]);
	ASSERT_FALSE(nodes_seen[6]);
	ASSERT_TRUE(nodes_seen[7]);
	ASSERT_FALSE(nodes_seen[8]);
	ASSERT_FALSE(nodes_seen[126]);
	ASSERT_TRUE(nodes_seen[127]);

	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_net_write);
	RUN_TEST(test_net_read);
	RUN_TEST(test_net__send_nmt);
	RUN_TEST(test_net__request_device_type);
	RUN_TEST(test_net__gettime_ms);
	RUN_TEST(test_net__wait_for_bootup);
	RUN_TEST(test_net__wait_for_sdo);
	return r;
}

