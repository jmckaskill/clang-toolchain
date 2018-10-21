#pragma once
#include <stdint.h>

typedef struct zip_central_dir_end zip_central_dir_end;
typedef struct zip64_central_dir_end zip64_central_dir_end;
typedef struct zip64_central_dir_locator zip64_central_dir_locator;
typedef struct zip_file_header zip_file_header;
typedef struct zip_file_extra zip_file_extra;
typedef struct zip_local_header zip_local_header;

struct zip_central_dir_end {
	uint8_t sig[4];
	uint8_t disk_num[2];
	uint8_t dir_start_disk_num[2];
	uint8_t num_entries_this_disk[2];
	uint8_t num_entries[2];
	uint8_t dir_len[4];
	uint8_t dir_offset[4];
	uint8_t comment_len[2];
};

#define ZIP_CENTRAL_DIR_END_SIG 0x06054b50

struct zip64_central_dir_end {
	uint8_t sig[4];
	uint8_t end_record_size[8];
	uint8_t create_version[2];
	uint8_t extract_version[2];
	uint8_t disk_num[4];
	uint8_t dir_start_disk_num[4];
	uint8_t num_entries_this_disk[8];
	uint8_t num_entries[8];
	uint8_t dir_len[8];
	uint8_t dir_start[8];
};

#define ZIP64_CENTRAL_DIR_END_SIG 0x06064b50

struct zip64_central_dir_locator {
	uint8_t sig[4];
	uint8_t dir_start_disk_num[4];
	uint8_t dir_offset[8];
	uint8_t total_disks[4];
};

#define ZIP64_CENTRAL_DIR_LOCATOR_SIG 0x07064b50

struct zip_file_header {
	uint8_t sig[4];
	uint8_t create_version[2];
	uint8_t extract_version[2];
	uint8_t flags[2];
	uint8_t compression_method[2];
	uint8_t mtime[2];
	uint8_t mdate[2];
	uint8_t crc32[4];
	uint8_t compressed_len[4];
	uint8_t uncompressed_len[4];
	uint8_t name_len[2];
	uint8_t extra_len[2];
	uint8_t comment_len[2];
	uint8_t disk_num[2];
	uint8_t internal_attributes[2];
	uint8_t external_attributes[4];
	uint8_t header_off[4];
};

#define ZIP_FILE_HEADER_SIG 0x02014b50

struct zip_file_extra {
	uint8_t tag[2];
	uint8_t size[2];
};

#define ZIP64_FILE_EXTRA 0x0001

struct zip64_extra {
	struct zip_file_extra h;
	uint8_t uncompressed_len[8];
	uint8_t compressed_len[8];
	uint8_t header_off[8];
};

struct zip_local_header {
	uint8_t sig[4];
	uint8_t extract_version[2];
	uint8_t flags[2];
	uint8_t compression_method[2];
	uint8_t mtime[2];
	uint8_t mdate[2];
	uint8_t crc32[4];
	uint8_t compressed_len[4];
	uint8_t uncompressed_len[4];
	uint8_t name_len[2];
	uint8_t extra_len[2];
};

#define ZIP_LOCAL_HEADER 0x04034b50

