#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#pragma pack(push)
#pragma pack(1)

enum MSG_TYPE
{
	TYPE_FIND_SERVICE,
	TYPE_RESPONSE_SERVICE,
	TYPE_VIDEO_DATA,
};

#define VIDEO_FRAME    20

#define BROADCAST_PORT 8888

#define SERVICE_NAME_LENGTH 256

static const char* magic_code_str = "creencast";

struct screen_cast_header_t
{
	uint8_t magic_code[16];
	uint32_t type;
	uint32_t length;
	screen_cast_header_t()
	{
		strcpy((char*)this->magic_code, magic_code_str);
	}
};

struct screen_cast_find_service_t : public screen_cast_header_t
{
	screen_cast_find_service_t()
	{
		this->type = TYPE_FIND_SERVICE;
		this->length = sizeof(screen_cast_find_service_t);
	}
};

struct screen_cast_response_t : public screen_cast_header_t
{
	uint8_t name[SERVICE_NAME_LENGTH];
	uint16_t tcp_port;
	screen_cast_response_t()
	{
		this->type = TYPE_RESPONSE_SERVICE;
		this->length = sizeof(screen_cast_response_t);
	}
};

struct screen_cast_video_data_t : public screen_cast_header_t
{
	uint8_t video_buffer[0];
};

#pragma pack(pop)

#endif