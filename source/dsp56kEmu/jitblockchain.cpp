#include "jitblockchain.h"

#include "dsp.h"
#include "jitasmjithelpers.h"
#include "jitblockruntimedata.h"
#include "jitblock.h"
#include "jitemitter.h"
#include "jitprofilingsupport.h"
#include "asmjit/core/jitruntime.h"
#include "jitblockemitter.h"

namespace dsp56k
{
	void funcRun(Jit* _jit, TWord _pc);
	void funcCreate(Jit* _jit, TWord _pc);
	void funcRecreate(Jit* _jit, TWord _pc);

	JitBlockChain::JitBlockChain(Jit& _jit, const JitDspMode& _mode) : m_jit(_jit), m_mode(_mode)
	{
		m_logger.reset(new AsmJitLogger());
		m_errorHandler.reset(new AsmJitErrorHandler());

		const auto& mem = _jit.dsp().memory();
		const auto pSize = mem.sizeP();
		m_jitCache.resize(pSize);
		m_jitFuncs.resize(pSize, &funcCreate);
	}

	JitBlockChain::~JitBlockChain()
	{
		for (auto& e : m_jitCache)
		{
			if(e.block)
				destroy(e.block);

			if(e.singleOpCache)
			{
				for (const auto& it : *e.singleOpCache)
					release(it.second);

				e.singleOpCache->clear();
			}
		}

		m_jitCache.clear();
	}

	bool JitBlockChain::canBeDefaultExecuted(TWord _pc) const
	{
		const auto& e = m_jitCache[_pc];
		if (!e.block)
			return false;
		return m_jitFuncs[_pc] == e.block->getFunc() || m_jitFuncs[_pc] == &funcRun;
	}

	void JitBlockChain::occupyArea(JitBlockRuntimeData* _block)
	{
		const auto first = _block->getPCFirst();
		const auto last = first + _block->getPMemSize();

		for (auto i = first; i < last; ++i)
		{
			assert(m_jitCache[i].block == nullptr || m_jitCache[i].block == _block);
			m_jitCache[i].block = _block;
			if (i == first)
				m_jitFuncs[i] = Jit::updateRunFunc(m_jitCache[i]);
			else
				m_jitFuncs[i] = &funcRecreate;
		}

		m_jit.addLoop(_block->getInfo());
	}

	void JitBlockChain::unoccupyArea(const JitBlockRuntimeData* _block)
	{
		const auto first = _block->getPCFirst();
		const auto last = first + _block->getPMemSize();

		for(auto i=first; i<last; ++i)
		{
			m_jitCache[i].block = nullptr;
			m_jitFuncs[i] = &funcCreate;
		}

		const auto& info = _block->getInfo();
		m_jit.removeLoop(info);
	}

	void JitBlockChain::create(const TWord _pc, bool _execute)
	{
//		LOG("Create @ " << HEX(_pc));// << std::endl << cacheEntry.block->getDisasm());

		auto& cacheEntry = m_jitCache[_pc];

		if(cacheEntry.singleOpCache)
		{
			TWord opA;
			TWord opB;
			m_jit.dsp().memory().getOpcode(_pc, opA, opB);

			// try to find two-word op first
			auto key = JitBlockRuntimeData::getSingleOpCacheKey(opA, opB);

			auto it = cacheEntry.findSingleOp(key);

			uint32_t cacheEntryLen = 2;

			// if not found, try one-word op
			if(!cacheEntry.isValid(it))
			{
				key = JitBlockRuntimeData::getSingleOpCacheKey(opA, JitBlockRuntimeData::SingleOpCacheIgnoreWordB);
				it = cacheEntry.findSingleOp(key);
				cacheEntryLen = 1;
			}

			if(cacheEntry.isValid(it))
			{
				if(cacheEntryLen == 1 || m_jitCache[_pc+1].block == nullptr)
				{
//					LOG("Returning single-op " << HEX(opA) << " at PC " << HEX(_pc));
					assert(cacheEntry.block == nullptr);

					cacheEntry.block = it->second;
					cacheEntry.removeSingleOp(it);

					occupyArea(cacheEntry.block);

					if(_execute)
						exec(_pc);
					return;
				}
			}
		}

		emit(_pc);
		if(_execute)
			exec(_pc);
	}

	void JitBlockChain::destroyParents(JitBlockRuntimeData* _block)
	{
		for (const auto parent : _block->getParents())
		{
			const auto& e = m_jitCache[parent];

			if (e.block)
				destroy(e.block);

			// single op cached entries that are calling the child block need to go, too. They have been created at a time where _block was not a volatile P block yet
			if(e.singleOpCache)
			{
				for(auto it = e.singleOpCache->begin(); it != e.singleOpCache->end();)
				{
					auto* b = it->second;
					if (b->getChild() == _block->getPCFirst() || b->getNonBranchChild() == _block->getPCFirst())
					{
						release(b);
						e.singleOpCache->erase(it++);
					}
					else
					{
						++it;
					}
				}
			}
		}
		_block->clearParents();
	}

	void JitBlockChain::destroy(JitBlockRuntimeData* _block)
	{
		destroyParents(_block);

		unoccupyArea(_block);

		if(m_jit.getConfig().cacheSingleOpBlocks && _block->getPMemSize() <= 2 && _block->getEncodedInstructionCount() == 1)
		{
			// if a single-word-op, cache it
			const auto first = _block->getPCFirst();
			auto& cacheEntry = m_jitCache[first];
			const auto op = _block->getSingleOpCacheKey();

			if(!cacheEntry.containsSingleOp(op))
			{
//				LOG("Caching single-op block " << HEX(op) << " at PC " << HEX(first));

				cacheEntry.addSingleOp(op, _block);
				return;
			}
		}

//		LOG("Destroying JIT block at PC " << HEX(first) << ", length " << _block->getPMemSize() << ", asm = " << _block->getDisasm());

		release(_block);
	}

	void JitBlockChain::destroy(const TWord _pc)
	{
		const auto block = m_jitCache[_pc].block;
		if (block)
			destroy(block);
	}

	void JitBlockChain::release(JitBlockRuntimeData* _block)
	{
#if DSP56300_DEBUGGER
		auto* d = m_jit.dsp().getDebugger();
		if(d)
			d->onJitBlockDestroyed(m_mode, _block->getInfo().pc);
#endif
		assert(m_codeSize >= _block->codeSize());
		m_codeSize -= _block->codeSize();
		m_jit.getRuntime()->release(_block->getFunc());

		m_jit.releaseBlockRuntimeData(_block);

//		LOG("Total code size now " << (m_codeSize >> 10) << "kb");
	}

	void JitBlockChain::recreate(const TWord _pc)
	{
		// there is code, but the JIT block does not start at the PC position that we want to run. We need to throw the block away and regenerate
//		LOG("Unable to jump into the middle of a block, destroying existing block & recreating from " << HEX(pc));
		m_jit.destroy(_pc);
		create(_pc, true);
	}

	JitBlockRuntimeData* JitBlockChain::getChildBlock(JitBlockRuntimeData* _parent, TWord _pc, bool _allowCreate/* = true*/)
	{
		if (!m_jit.getConfig().linkJitBlocks)
			return nullptr;

		if (_parent)
			occupyArea(_parent);

		if (m_jit.isVolatileP(_pc))
			return nullptr;

		const auto& e = m_jitCache[_pc];

		if (e.block && e.block->getPCFirst() == _pc)
		{
			// block is still being generated (circular reference)
			if (m_jitFuncs[_pc] == nullptr)
				return nullptr;

			if (!canBeDefaultExecuted(_pc))
				return nullptr;

			return e.block;
		}

		if (!_allowCreate)
			return nullptr;

		// If we jump into the middle of an existing block, this block needs to be regenerated.
		// However, we can only destroy blocks that are not part of the recursive generation that is running at the moment
		if (isBeingGeneratedRecursive(e.block))
			return nullptr;

		// regenerate otherwise
		if (e.block)
			destroy(e.block);

		create(_pc, false);

		if (!canBeDefaultExecuted(_pc))
			return nullptr;
		return e.block;
	}

	JitBlockRuntimeData* JitBlockChain::emit(TWord _pc)
	{
//		AsmJitLogger m_logger;
//		m_logger.addFlags(asmjit::FormatFlags::kHexImms | /*asmjit::FormatFlags::kHexOffsets |*/ asmjit::FormatFlags::kMachineCode);
//		code.setLogger(&m_logger);

//		m_asm.addDiagnosticOptions(DiagnosticOptions::kValidateIntermediate);
//		m_asm.addDiagnosticOptions(DiagnosticOptions::kValidateAssembler);

		auto* emitter = m_jit.acquireEmitter();

		emitter->codeHolder.setErrorHandler(m_errorHandler.get());
		emitter->codeHolder.init(m_jit.getRuntime()->environment());
		emitter->codeHolder.attach(&emitter->emitter);

		auto* b = m_jit.acquireBlockRuntimeData();

		m_errorHandler->setBlock(b);

		m_generatingBlocks.insert(std::make_pair(_pc, b));

		if(!emitter->block.emit(*b, this, _pc, m_jitCache, m_jit.getVolatileP(), m_jit.getLoops(), m_jit.getLoopEnds(), m_jit.getProfilingSupport()))
		{
			LOG("FATAL: code generation failed for PC " << HEX(_pc));
			m_jit.releaseBlockRuntimeData(b);
			m_generatingBlocks.erase(_pc);
			m_jit.releaseEmitter(emitter);
			return nullptr;
		}

		m_generatingBlocks.erase(_pc);

		emitter->emitter.ret();

		emitter->emitter.finalize();

		TJitFunc func;

		const auto err = m_jit.getRuntime()->add(&func, &emitter->codeHolder);

		if(err)
		{
			const auto* const errString = asmjit::DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			m_jit.releaseEmitter(emitter);
			return nullptr;
		}

		b->finalize(func, emitter->codeHolder);
		m_codeSize += emitter->codeHolder.codeSize();

		m_jit.releaseEmitter(emitter);

//		LOG("Total code size now " << (m_codeSize >> 10) << "kb");

		occupyArea(b);

		auto* profiling = m_jit.getProfilingSupport();
		if (profiling)
			profiling->addJitBlock(*b);

#if DSP56300_DEBUGGER
		auto* d = m_jit.dsp().getDebugger();
		if(d)
			d->onJitBlockCreated(m_mode, b);
#endif
		return b;
	}

	bool JitBlockChain::isBeingGeneratedRecursive(const JitBlockRuntimeData* _block) const
	{
		if (!_block)
			return false;

		if (isBeingGenerated(_block))
			return true;

		for (const auto parent : _block->getParents())
		{
			const auto& e = m_jitCache[parent];
			if (isBeingGeneratedRecursive(e.block))
				return true;
		}
		return false;
	}

	bool JitBlockChain::isBeingGenerated(const JitBlockRuntimeData* _block) const
	{
		if (_block == nullptr)
			return false;

		for (const auto& it : m_generatingBlocks)
		{
			if (it.second == _block)
				return true;
		}
		return false;
	}
}
