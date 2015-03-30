#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>

#include <linux/can.h>
#include "canopen.h"
#include "socketcan.h"
#include "canopen/byteorder.h"
#include "canopen/sdo_client.h"
#include "canopen/byteorder.h"

enum dict_entry_type {
	DICT_ENTRY_TYPE_UNKNOWN = -1,
	DICT_ENTRY_TYPE_U = 0x10,
	DICT_ENTRY_TYPE_U8 = 0x11,
	DICT_ENTRY_TYPE_U16 = 0x12,
	DICT_ENTRY_TYPE_U32 = 0x14,
	DICT_ENTRY_TYPE_U64 = 0x18,
	DICT_ENTRY_TYPE_S = 0x20,
	DICT_ENTRY_TYPE_S8 = 0x21,
	DICT_ENTRY_TYPE_S16 = 0x22,
	DICT_ENTRY_TYPE_S32 = 0x24,
	DICT_ENTRY_TYPE_S64 = 0x28,
	DICT_ENTRY_TYPE_VISIBLE_STRING = 0x30
};

enum dict_entry_access {
	DICT_ENTRY_ACCESS_R = 1,
	DICT_ENTRY_ACCESS_W = 2,
	DICT_ENTRY_ACCESS_RW = 3,
};

struct csv_line {
	int index;
	int subindex;

	enum dict_entry_access access;

	enum dict_entry_type type;
	union {
		unsigned long long unsigned_value;
		long long signed_value;
		const char* str_value;
	};
};

static inline int get_dict_entry_size(enum dict_entry_type type)
{
	return type & 0xf;
}

static inline int is_dict_entry_signed(enum dict_entry_type type)
{
	return (type & 0xf0) == DICT_ENTRY_TYPE_S;
}

static inline int is_prefix(const char* str, const char* prefix)
{
	return 0 == strncmp(str, prefix, strlen(prefix));
}

static inline int is_match(const char* a, const char* b)
{
	return 0 == strcmp(a, b);
}

static enum dict_entry_type str_to_type(const char* str)
{
	if (is_prefix(str, "UNSIGNED_"))
		return DICT_ENTRY_TYPE_U + atoi(&str[strlen("UNSIGNED_")]);

	if (is_prefix(str, "INTEGER_"))
		return DICT_ENTRY_TYPE_S + atoi(&str[strlen("INTEGER_")]);

	if (is_match(str, "VISIBLE_STRING"))
		return DICT_ENTRY_TYPE_VISIBLE_STRING;

	fprintf(stderr, "Unknown type: %s\n", str);
	return DICT_ENTRY_TYPE_UNKNOWN;
}

static enum dict_entry_access str_to_access(const char* str)
{
	if (is_match(str, "R"))  return DICT_ENTRY_ACCESS_R;
	if (is_match(str, "W"))  return DICT_ENTRY_ACCESS_W;
	if (is_match(str, "RW")) return DICT_ENTRY_ACCESS_RW;
	if (is_match(str, ""))   return 0;

	fprintf(stderr, "Unknown access type: %s\n", str);
	return 0;
}

static void usage()
{
	fprintf(stderr, "Usage: dlsdo <interface> <node id> <csv path>\n");
}

static int set_canopen_sdo_filter(int fd, int nodeid)
{
	struct can_filter filter;

	filter.can_mask = CAN_SFF_MASK;
	filter.can_id = R_TSDO + nodeid;

	return socketcan_apply_filters(fd, &filter, 1);
}

static inline int lenze_code_to_index(int code)
{
	return 0x5FFF - code;
}

/*
 *  1: Code;
 *  2: Subcode;
 *  3: Value;
 *  4: RawValue;
 *  5: RawHexValue;
 *  6: Parameter Type;
 *  7: Text;
 *  8: Unit;
 *  9: Min;
 * 10: Max;
 * 11: FactoryPresetValue;
 * 12: ControllerInhibit;
 * 13: Access
 */
static int unpack_csv_line(struct csv_line* output, char* line)
{
	const char* delim = ";\r\n";
	const char* value = NULL;

	char* tok = strtok(line, delim); /* 1 */
	if (!tok) return -1;
	if (!isdigit(*tok)) return -1;
	output->index = lenze_code_to_index(atoi(tok));

	tok = strtok(NULL, delim); /* 2 */
	if (!tok) return -1;
	output->subindex = atoi(tok);

	if (!strtok(NULL, delim)) return -1; /* 3 */

	value = strtok(NULL, delim); /* 4 */
	if (!value) return -1;

	if (!strtok(NULL, delim)) return -1; /* 5 */

	tok = strtok(NULL, delim); /* 6 */
	if (!tok) return -1;
	output->type = str_to_type(tok);
	if(output->type < 0)
		return -1;

	if (!strtok(NULL, delim)) return -1; /* 7 */
	if (!strtok(NULL, delim)) return -1; /* 8 */
	if (!strtok(NULL, delim)) return -1; /* 9 */
	if (!strtok(NULL, delim)) return -1; /* 10 */
	if (!strtok(NULL, delim)) return -1; /* 11 */
	if (!strtok(NULL, delim)) return -1; /* 12 */

	tok = strtok(NULL, delim); /* 13 */
	if (!tok) return -1;
	output->access = str_to_access(tok);

	if (output->type == DICT_ENTRY_TYPE_VISIBLE_STRING) {
		output->str_value = value;
	} else {
		if (is_dict_entry_signed(output->type))
			output->signed_value = strtoll(value, NULL, 0);
		else
			output->unsigned_value = strtoull(value, NULL, 0);
	}

	return 0;
}

static int write_can_frame(int fd, const struct can_frame* frame)
{
	if(write(fd, frame, sizeof(*frame)) == sizeof(*frame))
		return 0;

	perror("Warning: Dropped can frame in writing");

	return -1;
}

static int read_can_frame(struct can_frame* frame, int fd)
{
	ssize_t rsize = read(fd, frame, sizeof(*frame));
	if(rsize == sizeof(*frame))
		return 0;

	perror("Warning: Dropped can frame in writing");

	return -1;
}

static void download_single_buffer(int fd, struct csv_line* params,
				   const void* buffer, size_t size)
{
	struct can_frame rcf;
	struct sdo_dl_req req;

	sdo_request_download(&req, params->index, params->subindex, buffer,
			     size);

	do {
		if (write_can_frame(fd, &req.frame) < 0) return;
		if (read_can_frame(&rcf, fd) < 0) return;
		if (!sdo_dl_req_feed(&req, &rcf)) return;
	} while (req.state != SDO_REQ_DONE || req.state == SDO_REQ_ABORTED);

	if (req.state == SDO_REQ_ABORTED)
		fprintf(stderr, "Remote abort on index %d, subindex %d!\n",
				params->index, params->subindex);
}

static void download_single_string(int fd, struct csv_line* params)
{
	download_single_buffer(fd, params, params->str_value,
			       strlen(params->str_value));
}

static void download_single_number(int fd, struct csv_line* params)
{
	char buffer[8];

	switch (params->type) {
	case DICT_ENTRY_TYPE_U8:
		BYTEORDER(buffer, (uint8_t)params->unsigned_value);
		break;
	case DICT_ENTRY_TYPE_U16:
		BYTEORDER(buffer, (uint16_t)params->unsigned_value);
		break;
	case DICT_ENTRY_TYPE_U32:
		BYTEORDER(buffer, (uint32_t)params->unsigned_value);
		break;
	case DICT_ENTRY_TYPE_U64:
		BYTEORDER(buffer, (uint64_t)params->unsigned_value);
		break;
	case DICT_ENTRY_TYPE_S8:
		BYTEORDER(buffer,  (int8_t)params->signed_value);
		break;
	case DICT_ENTRY_TYPE_S16:
		BYTEORDER(buffer, (int16_t)params->signed_value);
		break;
	case DICT_ENTRY_TYPE_S32:
		BYTEORDER(buffer, (int32_t)params->signed_value);
		break;
	case DICT_ENTRY_TYPE_S64:
		BYTEORDER(buffer, (int64_t)params->signed_value);
		break;
	default:
		abort();
		break;
	}

	download_single_buffer(fd, params, buffer,
			       get_dict_entry_size(params->type));
}

static void download_single_parameter(int fd, struct csv_line* params)
{

	if (!(params->access & DICT_ENTRY_ACCESS_W))
		return;

	if(params->type == DICT_ENTRY_TYPE_VISIBLE_STRING)
		download_single_string(fd, params);
	else
		download_single_number(fd, params);
}

static int download_all_parameters(int fd, FILE* csv_file)
{
	ssize_t line_length = 0;
	size_t buffer_size = 0;
	char* line = NULL;
	struct csv_line params;

	while ((line_length = getline(&line, &buffer_size, csv_file)) > 0)
		if(unpack_csv_line(&params, line) == 0)
			download_single_parameter(fd, &params);

	free(line);
	return 0;
}

int main(int argc, char* argv[])
{
	int fd = 0;
	int nodeid = 0;
	FILE* csv_file = NULL;

	if (argc < 4) {
		usage();
		return 1;
	}

	const char* interface = argv[1];
	const char* nodeid_str = argv[2];
	const char* csv_path = argv[3];

	errno = 0;
	nodeid = strtoul(nodeid_str, NULL, 0);
	if (errno) {
		fprintf(stderr, "Invalid node id '%s': %s\n", nodeid_str,
				strerror(errno));
		usage();
		return 1;
	}

	if (nodeid > 127) {
		fprintf(stderr, "Node id must be less than 128\n");
		return 1;
	}

	csv_file = fopen(csv_path, "r");
	if (!csv_file) {
		fprintf(stderr, "Could not open csv file '%s': %s\n", csv_path,
				strerror(errno));
		return 1;
	}

	fd = socketcan_open(interface);
	if (fd < 0) {
		perror("Failed to open can interface");
		goto failure;
	}

	if (set_canopen_sdo_filter(fd, nodeid) < 0) {
		perror("Failed to apply can filters");
		goto failure;
	}

	if (download_all_parameters(fd, csv_file) < 0)
		goto failure;

	fclose(csv_file);
	close(fd);
	return 0;

failure:
	if(fd) close(fd);
	if(csv_file) fclose(csv_file);
	return 1;
}

