/*
 *  Phusion Passenger - https://www.phusionpassenger.com/
 *  Copyright (c) 2014-2015 Phusion
 *
 *  "Phusion Passenger" is a trademark of Hongli Lai & Ninh Bui.
 *
 *  See LICENSE file for license information.
 */
#ifndef _PASSENGER_SERVER_KIT_CONTEXT_H_
#define _PASSENGER_SERVER_KIT_CONTEXT_H_

#include <boost/make_shared.hpp>
#include <string>
#include <cstddef>
#include <jsoncpp/json.h>
#include <MemoryKit/mbuf.h>
#include <SafeLibev.h>
#include <Constants.h>
#include <Utils/StrIntUtils.h>
#include <Utils/JsonUtils.h>

extern "C" {
	struct uv_loop_s;
}

namespace Passenger {
namespace ServerKit {


struct FileBufferedChannelConfig {
	string bufferDir;
	unsigned int threshold;
	unsigned int delayInFileModeSwitching;
	unsigned int maxDiskChunkReadSize;
	bool autoTruncateFile;
	bool autoStartMover;

	FileBufferedChannelConfig()
		: bufferDir("/tmp"),
		  threshold(DEFAULT_FILE_BUFFERED_CHANNEL_THRESHOLD),
		  delayInFileModeSwitching(0),
		  maxDiskChunkReadSize(0),
		  autoTruncateFile(true),
		  autoStartMover(true)
		{ }
};

class Context {
private:
	void initialize() {
		mbuf_pool.mbuf_block_chunk_size = DEFAULT_MBUF_CHUNK_SIZE;
		MemoryKit::mbuf_pool_init(&mbuf_pool);
	}

public:
	SafeLibevPtr libev;
	struct uv_loop_s *libuv;
	struct MemoryKit::mbuf_pool mbuf_pool;
	string secureModePassword;
	FileBufferedChannelConfig defaultFileBufferedChannelConfig;

	Context(const SafeLibevPtr &_libev, struct uv_loop_s *_libuv)
		: libev(_libev),
		  libuv(_libuv)
	{
		initialize();
	}

	Context(struct ev_loop *loop)
		: libev(boost::make_shared<SafeLibev>(loop))
	{
		initialize();
	}

	~Context() {
		MemoryKit::mbuf_pool_deinit(&mbuf_pool);
	}

	Json::Value inspectStateAsJson() const {
		Json::Value doc;
		Json::Value mbufDoc;

		mbufDoc["free_blocks"] = (Json::UInt) mbuf_pool.nfree_mbuf_blockq;
		mbufDoc["active_blocks"] = (Json::UInt) mbuf_pool.nactive_mbuf_blockq;
		mbufDoc["chunk_size"] = (Json::UInt) mbuf_pool.mbuf_block_chunk_size;
		mbufDoc["offset"] = (Json::UInt) mbuf_pool.mbuf_block_offset;
		mbufDoc["spare_memory"] = byteSizeToJson(mbuf_pool.nfree_mbuf_blockq
			* mbuf_pool.mbuf_block_chunk_size);
		mbufDoc["active_memory"] = byteSizeToJson(mbuf_pool.nactive_mbuf_blockq
			* mbuf_pool.mbuf_block_chunk_size);
		#ifdef MBUF_ENABLE_DEBUGGING
			struct MemoryKit::active_mbuf_block_list *list =
				const_cast<struct MemoryKit::active_mbuf_block_list *>(
					&mbuf_pool.active_mbuf_blockq);
			struct MemoryKit::mbuf_block *block;
			Json::Value listJson(Json::arrayValue);

			TAILQ_FOREACH (block, list, active_q) {
				Json::Value blockJson;
				blockJson["refcount"] = block->refcount;
				#ifdef MBUF_ENABLE_BACKTRACES
					blockJson["backtrace"] =
						(block->backtrace == NULL)
						? "(null)"
						: block->backtrace;
				#endif
				listJson.append(blockJson);
			}
			mbufDoc["active_blocks_list"] = listJson;
		#endif

		doc["mbuf_pool"] = mbufDoc;

		return doc;
	}
};


} // namespace ServerKit
} // namespace Passenger

#endif /* _PASSENGER_SERVER_KIT_CONTEXT_H_ */