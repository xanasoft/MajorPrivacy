/*
* MIT License
*
* Copyright (c) 2025 David Xanatos (xanasoft.com)
* 
* Based on https://github.com/osesov/dnsfilter by Oleg Sesov Copyright (c) 2020
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "pch.h"
#include "DnsPacket.h"

std::ostream& operator<< (std::ostream& ostream, std::function< void(std::ostream&)> f)
{
	f(ostream);
	return ostream;
}

namespace DNS
{
	namespace
	{

		bool read16(const uint8_t *& data, const uint8_t * end, uint16_t & result)
		{
			if (end - data < 2)
				return false;
			result = (data[0] << 8u) | data[1];
			data += 2;
			return true;
		}

		bool read32(const uint8_t *& data, const uint8_t * end, uint32_t & result)
		{
			if (end - data < 4)
				return false;
			result = (data[0] << 24u) | (data[1] << 16u) | (data[2] << 8u) | data[3];
			data += 4;
			return true;
		}

		bool read_reference(
			uint16_t offset,
			const uint8_t * origin,
			const uint8_t * end,
			Name & name)
		{
			size_t size = end - origin;
			size_t last_offset = 0;
			// todo: detect loops

			while (true) {
				if (offset >= size)
					return false;

				uint8_t len = origin[offset++];
				if ((len & 0xc0u) == 0xc0u) {
					uint16_t off = len & 0x3f;

					if (offset == size)
						return false;
					off = (off << 8) | (origin[offset]);
					if (last_offset && off == last_offset) { // reference loop
						return false;
					}

					last_offset = offset = off;
					continue;
				}

				else if (len == 0)
					return true;

				if (len > size - offset)
					return false;

				name.emplace_back(origin + offset, origin + offset + len);

				offset += len;
			}
		}

		bool read_name(
			const uint8_t *& data,
			const uint8_t * origin,
			const uint8_t * end,
			Name& name)
		{
			while (true) {
				if (data == end)
					return false;

				uint8_t len = *data++;
				if ((len & 0xc0u) == 0xc0u) {
					uint16_t off = len & 0x3f;

					if (data == end)
						return false;
					off = (off << 8) | (*data++);
					return read_reference(off, origin, end, name);
				}

				else if (len == 0)
					return true;

				if (len > end - data)
					return false;

				name.emplace_back(data, data + len);
				data += len;
			}
		}

		bool read_query(
			const uint8_t *& data,
			const uint8_t * origin,
			const uint8_t * end,
			Query& query)
		{
			if (!read_name(data, origin, end, query.q_name))
				return false;

			if (!read16(data, end, (uint16_t&)query.q_type))
				return false;

			if (!read16(data, end, (uint16_t&)query.q_class))
				return false;

			return true;
		}

		bool read_reply(
			const uint8_t *& data,
			const uint8_t * origin,
			const uint8_t * end,
			Answer& answer)
		{
			// Read the name, type, class, TTL as before.
			if (!read_name(data, origin, end, answer.r_name))
				return false;

			if (!read16(data, end, (uint16_t&)answer.r_type))
				return false;

			if (!read16(data, end, (uint16_t&)answer.r_class))
				return false;

			if (!read32(data, end, answer.r_ttl))
				return false;

			uint16_t r_length;
			if (!read16(data, end, r_length))
				return false;
			if (r_length > end - data)
				return false;

			// Save the start of r_data.
			const uint8_t * rdata_start = data;
			Binary uncompressed;

			// Depending on the record type, process r_data accordingly.
			switch(answer.r_type)
			{
				// For records that do not contain any embedded names,
				// simply copy the raw r_data.
			case Type::A:
			case Type::AAAA:
			case Type::WKS:
			case Type::TXT:
			case Type::ANY:
				uncompressed.assign(data, data + r_length);
				break;

				// For a single domain name (e.g. CNAME, PTR, or NS/NA) we
				// decompress the name and then reassemble it in uncompressed form.
			case Type::CNAME:
			case Type::PTR:
			case Type::NA: // assuming NA is used as NS
			{
				Name decompressedName;
				const uint8_t * p = rdata_start;
				if (!read_name(p, origin, end, decompressedName))
					return false;
				for (const auto & label : decompressedName) {
					uint8_t len = static_cast<uint8_t>(label.size());
					uncompressed.push_back(len);
					uncompressed.insert(uncompressed.end(), label.begin(), label.end());
				}
				uncompressed.push_back(0);
				break;
			}

			// MX records: 2 bytes preference, followed by a domain name.
			case Type::MX:
			{
				if (r_length < 2)
					return false;
				// Copy the 2-byte preference.
				uncompressed.push_back(rdata_start[0]);
				uncompressed.push_back(rdata_start[1]);
				const uint8_t * p = rdata_start + 2;
				Name decompressedName;
				if (!read_name(p, origin, end, decompressedName))
					return false;
				for (const auto & label : decompressedName) {
					uint8_t len = static_cast<uint8_t>(label.size());
					uncompressed.push_back(len);
					uncompressed.insert(uncompressed.end(), label.begin(), label.end());
				}
				uncompressed.push_back(0);
				break;
			}

			// SOA records: contain two domain names (MNAME and RNAME)
			// followed by five 32-bit integers.
			case Type::SOA:
			{
				const uint8_t * p = rdata_start;
				Name mname, rname;
				if (!read_name(p, origin, end, mname))
					return false;
				if (!read_name(p, origin, end, rname))
					return false;
				// Reassemble the MNAME in uncompressed form.
				for (const auto & label : mname) {
					uint8_t len = static_cast<uint8_t>(label.size());
					uncompressed.push_back(len);
					uncompressed.insert(uncompressed.end(), label.begin(), label.end());
				}
				uncompressed.push_back(0);
				// And the RNAME.
				for (const auto & label : rname) {
					uint8_t len = static_cast<uint8_t>(label.size());
					uncompressed.push_back(len);
					uncompressed.insert(uncompressed.end(), label.begin(), label.end());
				}
				uncompressed.push_back(0);
				// Finally, copy the five 32-bit integers (20 bytes).
				if (p + 20 > end)
					return false;
				uncompressed.insert(uncompressed.end(), p, p + 20);
				break;
			}

			// SRV records: 2 bytes priority, 2 bytes weight, 2 bytes port,
			// followed by a domain name.
			case Type::SRV:
			{
				if (r_length < 6)
					return false;
				uncompressed.insert(uncompressed.end(), rdata_start, rdata_start + 6);
				const uint8_t * p = rdata_start + 6;
				Name decompressedName;
				if (!read_name(p, origin, end, decompressedName))
					return false;
				for (const auto & label : decompressedName) {
					uint8_t len = static_cast<uint8_t>(label.size());
					uncompressed.push_back(len);
					uncompressed.insert(uncompressed.end(), label.begin(), label.end());
				}
				uncompressed.push_back(0);
				break;
			}

			// For any unknown types, fall back to the raw r_data.
			default:
				uncompressed.assign(data, data + r_length);
				break;
			}

			// Store the uncompressed data in the answer.
			answer.r_data = uncompressed;
			// Advance the data pointer by the original r_length.
			data += r_length;
			return true;
		}
	}

	bool parse(const uint8_t * origin, size_t size, Packet& packet)
	{
		const uint8_t * data = origin;
		const uint8_t * end = data + size;
		uint16_t FLAGS, QDCOUNT, ANCOUNT, NSCOUNT, ARCOUNT;

		if (!read16(data, end, packet.ID)
			|| !read16(data, end, FLAGS)
			|| !read16(data, end, QDCOUNT)
			|| !read16(data, end, ANCOUNT)
			|| !read16(data, end, NSCOUNT)
			|| !read16(data, end, ARCOUNT))
		{
			return false;
		}

		packet.QR     = (FLAGS >> 15) & 1u;
		packet.OPCODE = (FLAGS >> 11) & 0xFu;
		packet.AA     = (FLAGS >> 10) & 0x1u;
		packet.TC     = (FLAGS >> 9) & 0x1u;
		packet.RD     = (FLAGS >> 8) & 0x1u;
		packet.RA     = (FLAGS >> 7) & 0x1u;
		packet.Z      = (FLAGS >> 4) & 0x7u;
		packet.RCODE  = (FLAGS >> 0) & 0xFu;

		for (uint16_t i = 0; i < QDCOUNT; ++i) {
			Query query;

			if (!read_query(data, origin, end, query))
				return false;

			packet.query.push_back(query);
		}

		for (uint16_t i = 0; i < ANCOUNT; ++i) {
			Answer answer;

			if (!read_reply(data, origin, end, answer))
				return false;

			packet.answer.push_back(answer);
		}

		for (uint16_t i = 0; i < NSCOUNT; ++i) {
			Answer answer;

			if (!read_reply(data, origin, end, answer))
				return false;

			packet.authority.push_back(answer);
		}

		for (uint16_t i = 0; i < ARCOUNT; ++i) {
			Answer answer;

			if (!read_reply(data, origin, end, answer))
				return false;

			packet.additional.push_back(answer);
		}

		if (data != end)
			return false;

		return true;
	}

	///////////////////////////////////////////////////////////
	using NameMap = std::map<std::string, uint16_t>;

	void write16(Binary& blob, uint16_t value)
	{
		blob.push_back((uint8_t)(value >> 8));
		blob.push_back((uint8_t)value);
	}

	void write32(Binary& blob, uint32_t value)
	{
		blob.push_back(value >> 24);
		blob.push_back(value >> 16);
		blob.push_back(value >> 8);
		blob.push_back(value);
	}

	std::string build_name(const Name& name, size_t off)
	{
		std::string n;
		for (size_t pos = off; pos < name.size(); ++pos) {
			if (pos != off)
				n.push_back('.');
			n.append(name[pos]);
		}
		return n;
	}

	void write_name(Binary& blob, const Name& name, NameMap& name_map)
	{
		for (size_t pos = 0; pos < name.size(); ++pos) {
			std::string n = build_name(name, pos);
			auto it = name_map.find(n);

			if (it == name_map.end()) { // not found, place
				size_t off = blob.size();
				if ((off & 0x3FFFu) == off)
					name_map.emplace(n, off);
				std::string cur_label = name[pos];

				uint8_t len = (uint8_t)cur_label.size();
				assert(len <= 63);

				blob.push_back(len);
				blob.insert(blob.end(), cur_label.begin(), cur_label.end());
			}
			else { // write reference
				size_t off = it->second;
				blob.push_back(0xC0u | (uint8_t)(off >> 8));
				blob.push_back((uint8_t)off);
				return;
			}
		}

		blob.push_back(0);
	}


	void write_query(Binary& data, const Query& query, NameMap& name_map)
	{
		write_name(data, query.q_name, name_map);
		write16(data, (uint16_t&)query.q_type);
		write16(data, (uint16_t&)query.q_class);
	}

	void write_reply(Binary& data, const Answer& answer, NameMap& name_map)
	{
		uint16_t r_length = (uint16_t)answer.r_data.size();
		assert(r_length == answer.r_data.size());

		write_name(data, answer.r_name, name_map);
		write16(data, (uint16_t&)answer.r_type);
		write16(data, (uint16_t&)answer.r_class);
		write32(data, answer.r_ttl);
		write16(data, r_length);
		data.insert( data.end(), answer.r_data.begin(), answer.r_data.end());
	}

	Binary build_packet(const Packet& packet)
	{
		Binary data;
		NameMap name_map;
		uint16_t FLAGS;
		uint16_t QDCOUNT = (uint16_t)packet.query.size();
		uint16_t ANCOUNT = (uint16_t)packet.answer.size();
		uint16_t NSCOUNT = (uint16_t)packet.authority.size();
		uint16_t ARCOUNT = (uint16_t)packet.additional.size();

		FLAGS  = static_cast<uint16_t>(packet.QR)     << 15;
		FLAGS |= static_cast<uint16_t>(packet.OPCODE) << 11;
		FLAGS |= static_cast<uint16_t>(packet.AA)     << 10;
		FLAGS |= static_cast<uint16_t>(packet.TC)     << 9;
		FLAGS |= static_cast<uint16_t>(packet.RD)     << 8;
		FLAGS |= static_cast<uint16_t>(packet.RA)     << 7;
		FLAGS |= static_cast<uint16_t>(packet.Z)      << 4;
		FLAGS |= static_cast<uint16_t>(packet.RCODE)  << 0;

		write16(data, packet.ID);
		write16(data, FLAGS);
		write16(data, QDCOUNT);
		write16(data, ANCOUNT);
		write16(data, NSCOUNT);
		write16(data, ARCOUNT);

		for (uint16_t i = 0; i < QDCOUNT; ++i) {
			write_query(data, packet.query[i], name_map);
		}

		for (uint16_t i = 0; i < ANCOUNT; ++i) {
			write_reply(data, packet.answer[i], name_map);
		}

		for (uint16_t i = 0; i < NSCOUNT; ++i) {
			write_reply(data, packet.authority[i], name_map);
		}

		for (uint16_t i = 0; i < ARCOUNT; ++i) {
			write_reply(data, packet.additional[i], name_map);
		}

		return data;
	}

	std::string output_name(const DNS::Name& name)
	{
		std::ostringstream oss;
		for (auto it = name.begin(); it != name.end(); ++it) {
			if (it != name.begin())
				oss << "|";
			oss << *it;
		}
		return oss.str();
	}

	std::string output_data(const DNS::Binary& data)
	{
		std::ostringstream oss;
		auto fill = oss.fill();
		oss << std::setfill('0') << std::hex;
		for (auto it = data.begin(); it != data.end(); ++it) {
			oss << std::setw(2) << static_cast<unsigned>(*it);
		}
		oss << std::dec;
		oss.fill(fill);
		return oss.str();
	}

	std::string output_query(const std::vector<Query>& query, const std::string& indent)
	{
		std::ostringstream oss;
		for (auto& q : query) {
			oss << indent << "NAME:  " << output_name(q.q_name) << '\n';
			oss << indent << "TYPE:  " << (uint16_t&)q.q_type << '\n';
			oss << indent << "CLASS: " << (uint16_t&)q.q_class << '\n';
		}
		return oss.str();
	}

	std::string output_answer(const std::vector<Answer>& answer, const std::string& indent)
	{
		std::ostringstream oss;
		for (auto& q : answer) {
			oss << indent << "NAME:  " << output_name(q.r_name) << '\n';
			oss << indent << "TYPE:  " << (uint16_t&)q.r_type << '\n';
			oss << indent << "CLASS: " << (uint16_t&)q.r_class << '\n';
			oss << indent << "TTL: " << q.r_ttl << '\n';
			oss << indent << "DATA: " << output_data(q.r_data) << '\n';
		}
		return oss.str();
	}

	std::string output_packet(const Packet& packet, const std::string& indent)
	{
		std::ostringstream oss;

		oss << indent << "ID: " << packet.ID << '\n';
		oss << indent << "QR: " << packet.QR << ". query (0), or a response (1)" << '\n';
		oss << indent << "OPCODE: " << packet.OPCODE << ". Authoritative Answer" << '\n';
		oss << indent << "AA: " << packet.AA << '\n';
		oss << indent << "TC: " << packet.TC << ". TrunCation" << '\n';
		oss << indent << "RD: " << packet.RD << ". Recursion Desired" << '\n';
		oss << indent << "RA: " << packet.RA << ". Recursion Available" << '\n';
		oss << indent << "Z: " << packet.Z << ". Zero bit" << '\n';
		oss << indent << "RCODE: " << packet.RCODE << ". Response code 0=ok" << '\n';

		std::string subindent = indent + "  ";
		oss << indent << "QUERY [" << packet.query.size() << "]:\n" << output_query(packet.query, subindent);
		oss << indent << "ANSWER [" << packet.answer.size() << "]:\n" << output_answer(packet.answer, subindent);
		oss << indent << "AUTH [" << packet.authority.size() << "]:\n" << output_answer(packet.authority, subindent);
		oss << indent << "ADDT [" << packet.additional.size() << "]:\n" << output_answer(packet.additional, subindent);

		return oss.str();
	}

	//std::string get_address(const Answer& ans)
	//{
	//	if (ans.r_type == (uint16_t)Type::A) {
	//		if (ans.r_data.size() == 4) {
	//			return inet_ntoa(*(in_addr*)ans.r_data.data());
	//		}
	//	}
	//	return std::string("???");
	//}
}