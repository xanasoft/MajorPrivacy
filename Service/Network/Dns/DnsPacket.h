#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <functional>
#include <map>
#include <cassert>
#include <assert.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>

namespace DNS
{
	using Binary = std::vector<uint8_t>;

	using Name = std::vector<std::string>;

	enum class Type : uint16_t
	{
		None = 0,
		A = 1,
		NA = 2,
		CNAME = 5,
		SOA = 6,
		WKS = 11,
		PTR = 12,
		MX = 15,
		TXT = 16,
		AAAA = 28,
		SRV = 33,
		ANY = 255,
	};

	enum class Class : uint16_t
	{
		INet = 1, // IN
		CH = 3,
		HS = 4,
		NONE = 254,
		ANY = 255,
	};

	struct Query
	{
		Name  q_name;
		Type  q_type;
		Class q_class;
	};

	struct Answer
	{
		Name     r_name;
		Type     r_type;
		Class    r_class;
		uint32_t r_ttl;
		Binary   r_data;
	};

	struct Packet
	{
		uint16_t ID;
		unsigned QR 	: 1;
		unsigned OPCODE : 4;
		unsigned AA 	: 1;
		unsigned TC 	: 1;
		unsigned RD 	: 1;
		unsigned RA 	: 1;
		unsigned Z 	    : 3;
		unsigned RCODE 	: 4;

		std::vector< Query > query;
		std::vector< Answer > answer;
		std::vector< Answer > authority;
		std::vector< Answer > additional;
	};

	bool parse(const uint8_t * origin, size_t size, Packet& packet);
	std::string build_name(const Name& name, size_t off);
	Binary build_packet(const Packet& packet);
	//std::string get_address(const Answer& ans);

	std::string output_packet(const Packet& packet, const std::string& indent);
	std::string output_data(const DNS::Binary& data);
}
