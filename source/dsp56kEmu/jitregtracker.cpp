#include "jitregtracker.h"

#include "jitblock.h"
#include "jitemitter.h"

namespace dsp56k
{
	DSPRegTemp::DSPRegTemp(JitBlock& _block, const bool _acquire) : m_block(_block)
	{
		if(_acquire)
			acquire();
	}

	DSPRegTemp::~DSPRegTemp()
	{
		release();
	}

	void DSPRegTemp::acquire()
	{
		if(acquired())
			return;

		m_dspReg = m_block.dspRegPool().aquireTemp();
		m_reg = m_block.dspRegPool().get(m_dspReg, false, false);
		m_block.dspRegPool().lock(m_dspReg);
	}

	void DSPRegTemp::release()
	{
		if(!acquired())
			return;

		m_block.dspRegPool().unlock(m_dspReg);
		m_block.dspRegPool().releaseTemp(m_dspReg);
		m_dspReg = JitDspRegPool::DspRegInvalid;
	}

	AluReg::AluReg(JitBlock& _block, const TWord _aluIndex, bool readOnly/* = false*/, bool writeOnly/* = false*/)
	: m_block(_block)
	, m_reg(_block)
	, m_readOnly(readOnly)
	, m_writeOnly(writeOnly)
	, m_aluIndex(_aluIndex)
	{
		if(!writeOnly)
			m_block.regs().getALU(m_reg, _aluIndex);
	}

	AluReg::~AluReg()
	{
		if(!m_readOnly)
		{
			DspValue v(std::move(m_reg), DspValue::Temp56);
			m_block.regs().setALU(m_aluIndex, v);
		}
	}

	JitReg64 AluReg::get() const
	{
		return m_reg.get();
	}

	void AluReg::release()
	{
		m_reg.release();
	}

	AluRef::AluRef(JitBlock& _block, const TWord _aluIndex, const bool _read, const bool _write)
	: m_block(_block)
	, m_read(_read)
	, m_write(_write)
	, m_aluIndex(_aluIndex)
	{
		m_reg.reset();
	}

	AluRef::~AluRef()
	{
		auto& p = m_block.dspRegPool();

		if(p.isParallelOp() && m_write)
		{
			// ALU write registers stay locked
		}
		else
		{
			const auto dspReg = static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspA + m_aluIndex);
			p.unlock(dspReg);
		}
	}

	JitReg64 AluRef::get()
	{
		if(m_reg.isValid())
			return m_reg;

		auto& p = m_block.dspRegPool();

		JitRegGP r;

		const auto dspRegR = static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspA      + m_aluIndex);
		const auto dspRegW = static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspAwrite + m_aluIndex);

		if(p.isParallelOp() && m_write)
		{
			JitRegGP rRead;

			if(m_read)
			{
				rRead = p.get(dspRegR, true, false);
				p.lock(dspRegR);
			}

			r = p.get(dspRegW, false, true);

			if(!p.isLocked(dspRegW))
				p.lock(dspRegW);

			if(m_read)
			{
				m_block.asm_().mov(r, rRead);
				p.unlock(dspRegR);
			}
		}
		else
		{
			r = p.get(dspRegR, m_read, m_write);
			p.lock(dspRegR);
		}
		m_reg = r.as<JitReg64>();
		return m_reg;
	}

	PushGP::PushGP(JitBlock& _block, const JitReg64& _reg) : m_block(_block), m_reg(_reg), m_pushed(_block.dspRegPool().isInUse(_reg) || _reg == regDspPtr)
	{
		if(m_pushed)
			m_block.stack().push(_reg);
	}

	PushGP::~PushGP()
	{
		if(m_pushed)
			m_block.stack().pop(m_reg);
	}

	PushXMM::PushXMM(JitBlock& _block, uint32_t _xmmIndex) : m_block(_block), m_xmmIndex(_xmmIndex), m_isLoaded(m_block.dspRegPool().isInUse(JitReg128(_xmmIndex)))
	{
		if(!m_isLoaded)
			return;

		const auto xm = JitReg128(_xmmIndex);

		_block.stack().push(xm);
	}

	PushXMM::~PushXMM()
	{
		if(!m_isLoaded)
			return;

		const auto xm = JitReg128(m_xmmIndex);

		m_block.stack().pop(xm);
	}

	PushXMMRegs::PushXMMRegs(JitBlock& _block) : m_block(_block)
	{
		for (const auto& xm : g_dspPoolXmms)
		{
			if (!xm.isValid())
				continue;
			if (!JitStackHelper::isNonVolatile(xm) && m_block.dspRegPool().isInUse(xm))
			{
				m_pushedRegs.push_front(xm);
				m_block.stack().push(xm);
			}
		}

		if (!JitStackHelper::isNonVolatile(regXMMTempA) && m_block.xmmPool().isInUse(regXMMTempA))
		{
			m_pushedRegs.push_front(regXMMTempA);
			_block.stack().push(regXMMTempA);
		}

		if(!JitStackHelper::isNonVolatile(regLastModAlu) && m_block.stack().isUsed(regLastModAlu))
		{
			m_pushedRegs.push_front(regLastModAlu);
			_block.stack().push(regLastModAlu);
		}
	}

	PushXMMRegs::~PushXMMRegs()
	{
		for (const auto& xm : m_pushedRegs)
			m_block.stack().pop(xm);
	}

	PushGPRegs::PushGPRegs(JitBlock& _block) : m_block(_block)
	{
		for (const auto& gp : g_dspPoolGps)
		{
			if (!JitStackHelper::isNonVolatile(gp) && !JitStackHelper::isFuncArg(gp) && m_block.dspRegPool().isInUse(gp))
			{
				m_pushedRegs.push_front(gp);
				_block.stack().push(r64(gp));
			}
		}
		for (auto reg : g_regGPTemps)
		{
			const auto gp = reg.as<JitRegGP>();

			if (!JitStackHelper::isNonVolatile(gp) && !JitStackHelper::isFuncArg(gp) && m_block.gpPool().isInUse(gp))
			{
				m_pushedRegs.push_front(gp);
				_block.stack().push(r64(gp));
			}
		}

		if(!JitStackHelper::isNonVolatile(regDspPtr) && !JitStackHelper::isFuncArg(regDspPtr))
		{
			m_pushedRegs.push_front(regDspPtr);
			_block.stack().push(regDspPtr);
		}

#ifdef HAVE_ARM64
		m_pushedRegs.push_front(asmjit::a64::regs::x30);
		_block.stack().push(asmjit::a64::regs::x30);
#endif
	}

	PushGPRegs::~PushGPRegs()
	{
		for (const auto& gp : m_pushedRegs)
		{
			m_block.stack().pop(gp);
		}
	}

	PushBeforeFunctionCall::PushBeforeFunctionCall(JitBlock& _block) : m_xmm(_block) , m_gp(_block)
	{
	}

	RegScratch::RegScratch(JitBlock& _block, const bool _acquire/* = true*/) : m_block(_block)
	{
		if(_acquire)
			acquire();
	}

	RegScratch::~RegScratch()
	{
		release();
	}

	void RegScratch::acquire()
	{
		if(m_acquired)
			return;
		m_block.lockScratch();
		m_acquired = true;
	}

	void RegScratch::release()
	{
		if(!m_acquired)
			return;
		m_block.unlockScratch();
		m_acquired = false;
	}

	JitRegpool::JitRegpool(std::initializer_list<JitReg> _availableRegs) : m_capacity(std::size(_availableRegs))
	{
		m_availableRegs.reserve(_availableRegs.size());

		for (auto it = std::rbegin(_availableRegs); it != std::rend(_availableRegs); ++it)
			m_availableRegs.push_back(*it);
	}

	void JitRegpool::put(JitScopedReg* _scopedReg, bool _weak)
	{
		if(_scopedReg->isWeak())
		{
			m_availableRegs.push_back(_scopedReg->get());
			m_weakRegs.remove(_scopedReg);
		}
		else
		{
			m_availableRegs.push_back(_scopedReg->get());
		}
	}

	JitReg JitRegpool::get(JitScopedReg* _scopedReg, bool _weak)
	{
		if(_weak)
		{
			assert(!m_availableRegs.empty() && "no more temporary registers left");

			const auto reg = m_availableRegs.back();
			m_availableRegs.pop_back();
			m_weakRegs.push_back(_scopedReg);
			return reg;
		}

		assert((!m_availableRegs.empty() || !m_weakRegs.empty()) && "no more temporary registers left");

		if(m_availableRegs.empty() && !m_weakRegs.empty())
		{
			m_weakRegs.front()->release();
			assert(!m_availableRegs.empty());
		}
		const auto ret = m_availableRegs.back();
		m_availableRegs.pop_back();
		return ret;
	}

	bool JitRegpool::empty() const
	{
		return m_availableRegs.empty();
	}

	bool JitRegpool::isInUse(const JitReg& _gp) const
	{
		return std::find(m_availableRegs.begin(), m_availableRegs.end(), _gp) == m_availableRegs.end();
	}

	void JitRegpool::reset(std::initializer_list<JitReg> _availableRegs)
	{
		m_weakRegs.clear();
		m_availableRegs.clear();

		for (auto it = std::rbegin(_availableRegs); it != std::rend(_availableRegs); ++it)
			m_availableRegs.push_back(*it);
	}

	JitScopedReg& JitScopedReg::operator=(JitScopedReg&& _other) noexcept
	{
		release();

		m_reg = _other.m_reg;
		m_acquired = _other.m_acquired;
		m_weak = _other.m_weak;

		_other.m_acquired = false;

		return *this;
	}

	void JitScopedReg::acquire()
	{
		if (m_acquired)
			return;
		m_reg = m_pool.get(this, m_weak);
		m_block.stack().setUsed(m_reg);
		m_acquired = true;
	}

	void JitScopedReg::release()
	{
		if (!m_acquired)
			return;
		m_pool.put(this, m_weak);
		m_acquired = false;
		m_reg.reset();
	}

	RegGP::RegGP(JitBlock& _block, const bool _acquire/* = true*/, const bool _weak/* = false*/) : JitScopedReg(_block, _block.gpPool(), _acquire, _weak)
	{
	}

	RegXMM::RegXMM(JitBlock& _block, const bool _acquire/* = true*/) : JitScopedReg(_block, _block.xmmPool(), _acquire)
	{
	}

	DSPReg::DSPReg(JitBlock& _block, const JitDspRegPool::DspReg _reg, const bool _read, const bool _write, const bool _acquire)
	: m_block(_block)
	, m_read(_read)
	, m_write(_write)
	, m_acquired(false)
	, m_locked(false)
	, m_dspReg(_reg)
	{
		if (_acquire)
			acquire();
	}

	DSPReg::DSPReg(DSPReg&& _other) noexcept
		: m_block(_other.m_block)
		, m_read(_other.m_read)
		, m_write(_other.m_write)
		, m_acquired(_other.m_acquired)
		, m_locked(_other.m_locked)
		, m_dspReg(_other.m_dspReg)
		, m_reg(_other.m_reg)
	{
		_other.m_acquired = false;
		_other.m_locked = false;
	}

	DSPReg::~DSPReg()
	{
		if(acquired())
			release();
	}

	void DSPReg::write()
	{
		assert(acquired());
		m_write = true;
		m_block.dspRegPool().get(m_dspReg, m_read, m_write);
	}

	void DSPReg::acquire()
	{
		assert(!acquired());

		m_reg = m_block.dspRegPool().get(m_dspReg, m_read, m_write);
		m_acquired = true;

		if(!m_block.dspRegPool().isLocked(m_dspReg))
		{
			m_block.dspRegPool().lock(m_dspReg);
			m_locked = true;
		}
	}

	void DSPReg::release()
	{
		if(m_locked)
		{
			m_block.dspRegPool().unlock(m_dspReg);
			m_locked = false;
		}

		m_reg.reset();
		m_acquired = false;
	}

	DSPReg& DSPReg::operator=(DSPReg&& _other) noexcept
	{
		if (m_dspReg != JitDspRegPool::DspRegInvalid && m_dspReg != _other.m_dspReg && m_acquired)
			release();

		m_read = _other.m_read;
		m_write = _other.m_write;
		m_acquired = _other.m_acquired;

		if(m_dspReg != JitDspRegPool::DspRegInvalid && m_dspReg == _other.m_dspReg && m_acquired && _other.m_acquired)
			m_locked |= _other.m_locked;
		else
			m_locked = _other.m_locked;

		m_dspReg = _other.m_dspReg;
		m_reg = _other.m_reg;

		_other.m_reg.reset();
		_other.m_acquired = false;
		_other.m_locked = false;

		return *this;
	}
}
