#pragma once

#include <cstdint>
#include <vector>

#include "jittypes.h"

namespace asmjit
{
	inline namespace _abi_1_9
	{
		class BaseNode;
	}
}

namespace dsp56k
{
	class JitBlock;

	class JitStackHelper
	{
	public:
		JitStackHelper(JitBlock& _block);
		~JitStackHelper();

		void push(const JitReg64& _reg);
		void push(const JitReg128& _reg);

		void pop(const JitReg64& _reg);
		void pop(const JitReg128& _reg);
		void pop(const JitReg& _reg);
		void pop();

		void popAll();

		void pushNonVolatiles();
		
		void call(const void* _funcAsPtr) const;

		void movePushesTo(asmjit::BaseNode* _baseNode, size_t _firstIndex);

		static bool isFuncArg(const JitRegGP& _gp);
		static bool isNonVolatile(const JitRegGP& _gp);
		static bool isNonVolatile(const JitReg128& _xm);

		void setUsed(const JitReg& _reg);
		void setUsed(const JitRegGP& _reg);
		void setUsed(const JitReg128& _reg);

		const auto& getUsedRegs() const { return m_usedRegs; }

		bool isUsed(const JitReg& _reg) const;

		uint32_t pushSize(const JitReg& _reg);

		uint32_t pushedSize() const { return m_pushedBytes; }
		size_t pushedRegCount() const { return m_pushedRegs.size(); }

		void reset();

	private:
		void stackRegAdd(uint64_t _offset) const;
		void stackRegSub(uint64_t _offset) const;

		JitBlock& m_block;
		uint32_t m_pushedBytes = 0;

		struct PushedReg
		{
			uint32_t stackOffset = 0;
			JitReg reg;
			asmjit::BaseNode* cursorFirst = nullptr;
			asmjit::BaseNode* cursorLast = nullptr;

			bool operator < (const PushedReg& _r) const
			{
				return stackOffset > _r.stackOffset;	// reversed as stack is downwards
			}
		};

		std::vector<PushedReg> m_pushedRegs;
		std::vector<JitReg> m_usedRegs;
	};

	class PushMover
	{
	public:
		explicit PushMover(JitBlock& _block, bool _begin = true);
		~PushMover();

		void begin();
		void end();
	private:
		JitBlock& m_block;
		asmjit::BaseNode* m_cursorBeforePushes = nullptr;
		size_t m_beginPushCount = 0;
	};
}
