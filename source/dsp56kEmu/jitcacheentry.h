#pragma once

#include <map>

#include "types.h"

namespace dsp56k
{
	class JitBlockRuntimeData;
	class Jit;

	typedef void (*TJitFunc)(Jit*, TWord);

	struct JitCacheEntry
	{
		using SingleOpMap = std::map<uint64_t, JitBlockRuntimeData*>;
		using SingleOpMapIt = SingleOpMap::iterator;

		~JitCacheEntry()
		{
			delete singleOpCache;
		}

		SingleOpMapIt findSingleOp(const uint64_t _key) const
		{
			if(!singleOpCache)
				return {};

			return singleOpCache->find(_key);
		}

		bool containsSingleOp(const uint64_t _key) const
		{
			return singleOpCache != nullptr && singleOpCache->find(_key) != singleOpCache->end();
		}

		void removeSingleOp(const SingleOpMapIt& _it)
		{
			if(!singleOpCache)
				return;
			singleOpCache->erase(_it);
		}

		void addSingleOp(const uint64_t _key, JitBlockRuntimeData* _block)
		{
			if(!singleOpCache)
				singleOpCache = new std::map<uint64_t, JitBlockRuntimeData*>();
			singleOpCache->insert(std::make_pair(_key, _block));
		}

		bool isValid(const SingleOpMapIt& _it) const
		{
			return singleOpCache != nullptr && _it != singleOpCache->end();
		}

		JitBlockRuntimeData* block = nullptr;
		std::map<uint64_t, JitBlockRuntimeData*>* singleOpCache = nullptr;
	};
}
