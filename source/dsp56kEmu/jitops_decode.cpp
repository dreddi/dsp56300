#include "jitops.h"
#include "types.h"

namespace dsp56k
{
	void JitOps::decode_dddddd_read(DspValue& _dst, const TWord _dddddd)
	{
		const auto i = _dddddd & 0x3f;
		switch( i )
		{
		// 0000DD - 4 registers in data ALU - NOT DOCUMENTED but the motorola disasm claims it works, for example for the lua instruction
		case 0x00:	getXY0(_dst, 0, false);	break;
		case 0x01:	getXY1(_dst, 0, false);	break;
		case 0x02:	getXY0(_dst, 1, false);	break;
		case 0x03:	getXY1(_dst, 1, false);	break;
		// 0001DD - 4 registers in data ALU
		case 0x04:	getXY0(_dst, 0, false);	break;
		case 0x05:	getXY1(_dst, 0, false);	break;
		case 0x06:	getXY0(_dst, 1, false);	break;
		case 0x07:	getXY1(_dst, 1, false);	break;

		// 001DDD - 8 accumulators in data ALU
		case 0x08:	getALU0(_dst, 0);	break;
		case 0x09:	getALU0(_dst, 1);	break;
		case 0x0a:
		case 0x0b:
			{
				const auto aluIndex = i - 0x0a;
				getALU2signed(_dst, aluIndex);
			}
			break;
		case 0x0c:	getALU1(_dst, 0);	break;
		case 0x0d:	getALU1(_dst, 1);	break;
		case 0x0e:	transferAluTo24(_dst, 0);	break;
		case 0x0f:	transferAluTo24(_dst, 1);	break;

		// 010TTT - 8 address registers in AGU
		case 0x10:	m_dspRegs.getR(_dst, 0); break;
		case 0x11:	m_dspRegs.getR(_dst, 1); break;
		case 0x12:	m_dspRegs.getR(_dst, 2); break;
		case 0x13:	m_dspRegs.getR(_dst, 3); break;
		case 0x14:	m_dspRegs.getR(_dst, 4); break;
		case 0x15:	m_dspRegs.getR(_dst, 5); break;
		case 0x16:	m_dspRegs.getR(_dst, 6); break;
		case 0x17:	m_dspRegs.getR(_dst, 7); break;

		// 011NNN - 8 address offset registers in AGU
		case 0x18:	m_dspRegs.getN(_dst, 0); break;
		case 0x19:	m_dspRegs.getN(_dst, 1); break;
		case 0x1a:	m_dspRegs.getN(_dst, 2); break;
		case 0x1b:	m_dspRegs.getN(_dst, 3); break;
		case 0x1c:	m_dspRegs.getN(_dst, 4); break;
		case 0x1d:	m_dspRegs.getN(_dst, 5); break;
		case 0x1e:	m_dspRegs.getN(_dst, 6); break;
		case 0x1f:	m_dspRegs.getN(_dst, 7); break;

		// 100FFF - 8 address modifier registers in AGU
		case 0x20:	m_dspRegs.getM(_dst, 0); break;
		case 0x21:	m_dspRegs.getM(_dst, 1); break;
		case 0x22:	m_dspRegs.getM(_dst, 2); break;
		case 0x23:	m_dspRegs.getM(_dst, 3); break;
		case 0x24:	m_dspRegs.getM(_dst, 4); break;
		case 0x25:	m_dspRegs.getM(_dst, 5); break;
		case 0x26:	m_dspRegs.getM(_dst, 6); break;
		case 0x27:	m_dspRegs.getM(_dst, 7); break;
		
		// 101EEE - 1 address register in AGU
		case 0x2a:	m_dspRegs.getEP(_dst);	break;

		// 110VVV - 2 program controller registers
		case 0x30:	m_dspRegs.getVBA(_dst);	break;
		case 0x31:	m_dspRegs.getSC(_dst);	break;

		// 111GGG - 8 program controller registers
		case 0x38:	m_dspRegs.getSZ(_dst);	break;
		case 0x39:	getSR(_dst);			break;
		case 0x3a:	m_dspRegs.getOMR(_dst);	break;
		case 0x3b:	m_dspRegs.getSP(_dst);	break;
		case 0x3c:	getSSH(_dst);			break;
		case 0x3d:	getSSL(_dst);			break;
		case 0x3e:	m_dspRegs.getLA(_dst);	break;
		case 0x3f:	m_dspRegs.getLC(_dst);	break;
		default:
			assert(0 && "invalid dddddd value");
		}
	}

	void JitOps::decode_dddddd_write(const TWord _dddddd, const DspValue& _src, bool _sourceIs8Bit/* = false*/)
	{
		DspValue vImm(m_block, _src.imm(), DspValue::Immediate8);

		const auto signedFraction = [&](const DspValue& _s) -> const DspValue&
		{
			if(_sourceIs8Bit)
			{
				assert(_s.isImmediate());
				vImm.convertSigned8To24();
				return vImm;
			}
			return _s;
		};

		const auto unsigned24 = [&](const DspValue& _s) -> const DspValue&
		{
			if (_sourceIs8Bit)
			{
				assert(_s.isImmediate());
				vImm.convertUnsigned8To24();
				return vImm;
			}
			return _s;
		};

		const auto i = _dddddd & 0x3f;
		switch( i )
		{
		// 0000DD - 4 registers in data ALU - NOT DOCUMENTED but the motorola disasm claims it works, for example for the lua instruction
		case 0x00:	setXY0(0, signedFraction(_src));	break;
		case 0x01:	setXY1(0, signedFraction(_src));	break;
		case 0x02:	setXY0(1, signedFraction(_src));	break;
		case 0x03:	setXY1(1, signedFraction(_src));	break;
		// 0001DD - 4 registers in data ALU
		case 0x04:	setXY0(0, signedFraction(_src));	break;
		case 0x05:	setXY1(0, signedFraction(_src));	break;
		case 0x06:	setXY0(1, signedFraction(_src));	break;
		case 0x07:	setXY1(1, signedFraction(_src));	break;

		// 001DDD - 8 accumulators in data ALU
		case 0x08:	setALU0(0, unsigned24(_src));	break;
		case 0x09:	setALU0(1, unsigned24(_src));	break;
		case 0x0a:	setALU2(0, unsigned24(_src));	break;
		case 0x0b:	setALU2(1, unsigned24(_src));	break;
		case 0x0c:	setALU1(0, unsigned24(_src));	break;
		case 0x0d:	setALU1(1, unsigned24(_src));	break;
		case 0x0e:	transfer24ToAlu(0, signedFraction(_src));	break;	
		case 0x0f:	transfer24ToAlu(1, signedFraction(_src));	break;

		// 010TTT - 8 address registers in AGU
		case 0x10:	m_dspRegs.setR(0, unsigned24(_src)); break;
		case 0x11:	m_dspRegs.setR(1, unsigned24(_src)); break;
		case 0x12:	m_dspRegs.setR(2, unsigned24(_src)); break;
		case 0x13:	m_dspRegs.setR(3, unsigned24(_src)); break;
		case 0x14:	m_dspRegs.setR(4, unsigned24(_src)); break;
		case 0x15:	m_dspRegs.setR(5, unsigned24(_src)); break;
		case 0x16:	m_dspRegs.setR(6, unsigned24(_src)); break;
		case 0x17:	m_dspRegs.setR(7, unsigned24(_src)); break;

		// 011NNN - 8 address offset registers in AGU
		case 0x18:	m_dspRegs.setN(0, unsigned24(_src)); break;
		case 0x19:	m_dspRegs.setN(1, unsigned24(_src)); break;
		case 0x1a:	m_dspRegs.setN(2, unsigned24(_src)); break;
		case 0x1b:	m_dspRegs.setN(3, unsigned24(_src)); break;
		case 0x1c:	m_dspRegs.setN(4, unsigned24(_src)); break;
		case 0x1d:	m_dspRegs.setN(5, unsigned24(_src)); break;
		case 0x1e:	m_dspRegs.setN(6, unsigned24(_src)); break;
		case 0x1f:	m_dspRegs.setN(7, unsigned24(_src)); break;

		// 100FFF - 8 address modifier registers in AGU
		case 0x20:	m_dspRegs.setM(0, unsigned24(_src)); break;
		case 0x21:	m_dspRegs.setM(1, unsigned24(_src)); break;
		case 0x22:	m_dspRegs.setM(2, unsigned24(_src)); break;
		case 0x23:	m_dspRegs.setM(3, unsigned24(_src)); break;
		case 0x24:	m_dspRegs.setM(4, unsigned24(_src)); break;
		case 0x25:	m_dspRegs.setM(5, unsigned24(_src)); break;
		case 0x26:	m_dspRegs.setM(6, unsigned24(_src)); break;
		case 0x27:	m_dspRegs.setM(7, unsigned24(_src)); break;

		// 101EEE - 1 address register in AGU
		case 0x2a:	m_dspRegs.setEP(_src);				break;

		// 110VVV - 2 program controller registers
		case 0x30:	m_dspRegs.setVBA(_src);				break;
		case 0x31:	m_dspRegs.setSC(_src);				break;

		// 111GGG - 8 program controller registers
		case 0x38:	m_dspRegs.setSZ(_src);				break;
		case 0x39:	setSR(_src);						break;
		case 0x3a:	m_dspRegs.setOMR(_src);				break;
		case 0x3b:	m_dspRegs.setSP(_src);				break;
		case 0x3c:	setSSH(_src);						break;
		case 0x3d:	setSSL(_src);						break;
		case 0x3e:	m_dspRegs.setLA(_src);	m_resultFlags |= WriteToLA; break;
		case 0x3f:	m_dspRegs.setLC(_src);	m_resultFlags |= WriteToLC; break;
		default:
			assert(0 && "invalid dddddd value");
		}
	}

	DspValue JitOps::decode_dddddd_ref(const TWord _dddddd, const bool _read, const bool _write) const
	{
		const auto i = _dddddd & 0x3f;

		auto makeRef = [&](JitDspRegPool::DspReg _reg)
		{
			return DspValue(m_block, _reg, _read, _write);
		};

		switch( i )
		{
		// 0000DD - 4 registers in data ALU - NOT DOCUMENTED but the motorola disasm claims it works, for example for the lua instruction
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		// 0001DD - 4 registers in data ALU
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:

		// 001DDD - 8 accumulators in data ALU
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
		case 0x0c:
		case 0x0d:  return DspValue(m_block);

		case 0x0e:	return makeRef(JitDspRegPool::DspA);
		case 0x0f:	return makeRef(JitDspRegPool::DspB);

		// 010TTT - 8 address registers in AGU
		case 0x10:	return makeRef(JitDspRegPool::DspR0);
		case 0x11:	return makeRef(JitDspRegPool::DspR1);
		case 0x12:	return makeRef(JitDspRegPool::DspR2);
		case 0x13:	return makeRef(JitDspRegPool::DspR3);
		case 0x14:	return makeRef(JitDspRegPool::DspR4);
		case 0x15:	return makeRef(JitDspRegPool::DspR5);
		case 0x16:	return makeRef(JitDspRegPool::DspR6);
		case 0x17:	return makeRef(JitDspRegPool::DspR7);

		// 011NNN - 8 address offset registers in AGU
		case 0x18:	return makeRef(JitDspRegPool::DspN0);
		case 0x19:	return makeRef(JitDspRegPool::DspN1);
		case 0x1a:	return makeRef(JitDspRegPool::DspN2);
		case 0x1b:	return makeRef(JitDspRegPool::DspN3);
		case 0x1c:	return makeRef(JitDspRegPool::DspN4);
		case 0x1d:	return makeRef(JitDspRegPool::DspN5);
		case 0x1e:	return makeRef(JitDspRegPool::DspN6);
		case 0x1f:	return makeRef(JitDspRegPool::DspN7);

		// 100FFF - 8 address modifier registers in AGU
		case 0x20:	return makeRef(JitDspRegPool::DspM0);
		case 0x21:	return makeRef(JitDspRegPool::DspM1);
		case 0x22:	return makeRef(JitDspRegPool::DspM2);
		case 0x23:	return makeRef(JitDspRegPool::DspM3);
		case 0x24:	return makeRef(JitDspRegPool::DspM4);
		case 0x25:	return makeRef(JitDspRegPool::DspM5);
		case 0x26:	return makeRef(JitDspRegPool::DspM6);
		case 0x27:	return makeRef(JitDspRegPool::DspM7);

		// 101EEE - 1 address register in AGU
		case 0x2a:

		// 110VVV - 2 program controller registers
		case 0x30:
		case 0x31:

		// 111GGG - 8 program controller registers
		case 0x38:	return DspValue(m_block);
		case 0x39:	return makeRef(JitDspRegPool::DspSR);
		case 0x3a:
		case 0x3b:
		case 0x3c:
		case 0x3d:	return DspValue(m_block);
		case 0x3e:	return makeRef(JitDspRegPool::DspLA);
		case 0x3f:	return makeRef(JitDspRegPool::DspLC);
		default:
			assert(0 && "invalid dddddd value");
		}
		return DspValue(m_block);
	}

	void JitOps::decode_ddddd_pcr_read(DspValue& _dst, TWord _ddddd)
	{
		if( (_ddddd & 0x18) == 0x00 )
		{
			return m_dspRegs.getM(_dst, _ddddd&0x07);
		}

		switch( _ddddd )
		{
		case 0xa:	m_dspRegs.getEP(_dst);	break;
		case 0x10:	m_dspRegs.getVBA(_dst);	break;
		case 0x11:	m_dspRegs.getSC(_dst);	break;
		case 0x18:	m_dspRegs.getSZ(_dst);	break;
		case 0x19:	getSR(_dst);			break;
		case 0x1a:	m_dspRegs.getOMR(_dst);	break;
		case 0x1b:	m_dspRegs.getSP(_dst);	break;
		case 0x1c:	getSSH(_dst);			break;
		case 0x1d:	getSSL(_dst);			break;
		case 0x1e:	m_dspRegs.getLA(_dst);	break;
		case 0x1f:	m_dspRegs.getLC(_dst);	break;
		default: assert( 0 && "invalid ddddd value" ); break;
		}
	}

	void JitOps::decode_ddddd_pcr_write(TWord _ddddd, const DspValue& _src)
	{
		if( (_ddddd & 0x18) == 0x00 )
		{
			m_dspRegs.setM(_ddddd&0x07, _src);
			return;
		}

		switch( _ddddd )
		{
		case 0xa:	m_dspRegs.setEP(_src);				break;
		case 0x10:	m_dspRegs.setVBA(_src);				break;
		case 0x11:	m_dspRegs.setSC(_src);				break;
		case 0x18:	m_dspRegs.setSZ(_src);				break;
		case 0x19:	setSR(_src);						break;
		case 0x1a:	m_dspRegs.setOMR(_src);				break;
		case 0x1b:	m_dspRegs.setSP(_src);				break;
		case 0x1c:	setSSH(_src);						break;
		case 0x1d:	setSSL(_src);						break;
		case 0x1e:	m_dspRegs.setLA(_src);	m_resultFlags |= WriteToLA; break;
		case 0x1f:	m_dspRegs.setLC(_src);	m_resultFlags |= WriteToLC; break;
		default:
			assert(0 && "invalid ddddd value");
		}
	}
	
	void JitOps::decode_ee_read(DspValue& _dst, const TWord _ee)
	{
		switch (_ee)
		{
		case 0: getX0(_dst, false); break;
		case 1: getX1(_dst, false); break;
		case 2: transferAluTo24(_dst, 0); break;
		case 3: transferAluTo24(_dst, 1); break;
		default: assert(0 && "invalid ee value");
		}
	}

	void JitOps::decode_ee_write(const TWord _ee, const DspValue& _value)
	{
		switch (_ee)
		{
		case 0: setXY0(0, _value); return;
		case 1: setXY1(0, _value); return;
		case 2: transfer24ToAlu(0, _value);	return;
		case 3: transfer24ToAlu(1, _value);	return;
		}
		assert(0 && "invalid ee value");
	}

	void JitOps::decode_ff_read(DspValue& _dst, TWord _ff)
	{
		switch (_ff)
		{
		case 0: getY0(_dst, false); break;
		case 1: getY1(_dst, false); break;
		case 2: transferAluTo24(_dst, 0); break;
		case 3: transferAluTo24(_dst, 1); break;
		default:
			assert(0 && "invalid ff value");
			break;
		}
	}

	void JitOps::decode_ff_write(const TWord _ff, const DspValue& _value)
	{
		switch (_ff)
		{
		case 0: setXY0(1, _value); break;
		case 1: setXY1(1, _value); break;
		case 2: transfer24ToAlu(0, _value); break;
		case 3: transfer24ToAlu(1, _value); break;
		default: assert(false && "invalid ff value"); break;
		}
	}

	void JitOps::decode_EE_read(RegGP& dst, TWord _ee)
	{
		switch (_ee)
		{
		case 0: return getMR(dst);
		case 1: {assert(false && "not reachable, ori and andi have special code for this"); getCCR(dst); break;}
		case 2: getCOM(dst); break;
		case 3: getEOM(dst); break;
		default:
			assert(0 && "invalid EE value");
			break;
		}
	}

	void JitOps::decode_EE_write(const JitReg64& _src, TWord _ee)
	{
		switch (_ee)
		{
		case 0: setMR(_src); break;
		case 1: {assert(false && "not reachable, ori and andi have special code for this"); setCCR(_src); break;}
		case 2: setCOM(_src); break;
		case 3: setEOM(_src); break;
		default:
			assert(0 && "invalid EE value");
			break;
		}
	}

	void JitOps::decode_JJJ_read_56(const JitReg64& _dst, const TWord _jjj, const bool _b) const
	{
		switch (_jjj)
		{
		case 0:
		case 1:			m_dspRegs.getALU(_dst, _b ? 1 : 0);			break;
		case 2:			XYto56(_dst, 0);							break;
		case 3: 		XYto56(_dst, 1);							break;
		case 4:			XY0to56(_dst, 0);							break;
		case 5: 		XY0to56(_dst, 1);							break;
		case 6:			XY1to56(_dst, 0);							break;
		case 7: 		XY1to56(_dst, 1);							break;
		default:
			assert(0 && "unreachable, invalid JJJ value");
		}
	}

	DspValue JitOps::decode_JJJ_read_56(const TWord _jjj, const bool _b) const
	{
		if(_jjj < 2)
			return DspValue(m_block, _b ? JitDspRegPool::DspReg::DspB : JitDspRegPool::DspReg::DspA, true, false);

		DspValue v(m_block);
		v.temp(DspValue::Temp56);
		const auto& r = r64(v.get());

		switch (_jjj)
		{
		case 0:
		case 1:
		case 2:			XYto56(r, 0);							break;
		case 3: 		XYto56(r, 1);							break;
		case 4:			XY0to56(r, 0);							break;
		case 5: 		XY0to56(r, 1);							break;
		case 6:			XY1to56(r, 0);							break;
		case 7: 		XY1to56(r, 1);							break;
		default:
			assert(0 && "unreachable, invalid JJJ value");
			return DspValue(m_block);
		}
		return DspValue(std::move(v));
	}

	void JitOps::decode_JJ_read(DspValue& _dst, TWord jj) const
	{
		switch (jj)
		{
		case 0:
		case 1:
			getXY0(_dst, jj, false);
			break;
		case 2: 
		case 3:
			getXY1(_dst, jj - 2, false);
			break;
		default:
			assert(0 && "unreachable, invalid JJ value");
		}
	}

	DspValue JitOps::decode_RRR_read(TWord _rrr, int _shortDisplacement/* = 0*/)
	{
		const auto rrr = _rrr & 0x07;

		if(_shortDisplacement)
		{
			DspValue dst(m_block);
			m_dspRegs.getR(dst, rrr);
			dst.toTemp();

			const RegGP n(m_block);
			m_asm.mov(r32(n), asmjit::Imm(std::abs(_shortDisplacement)));

			DspValue m(m_block);
			m_dspRegs.getM(m, rrr);

			updateAddressRegisterSub(r32(dst), r32(n.get()), r32(m.get()), rrr, _shortDisplacement > 0 ? true : false);
			
			return DspValue(std::move(dst));
		}

		return DspValue(m_block, JitDspRegPool::DspR0, true, false, rrr);
	}

	void JitOps::decode_qq_read(DspValue& _dst, TWord _qq, bool _signextend)
	{
		switch (_qq)
		{
		case 0: getX0(_dst, _signextend); break;
		case 1: getY0(_dst, _signextend); break;
		case 2: getX1(_dst, _signextend); break;
		case 3: getY1(_dst, _signextend); break;
		default: assert(0 && "invalid qq value"); break;
		}
	}

	void JitOps::decode_QQ_read(DspValue& _dst, TWord _qq, bool _signextend)
	{
		switch (_qq)
		{
		case 0: getY1(_dst, _signextend);	break;
		case 1: getX0(_dst, _signextend);	break;
		case 2: getY0(_dst, _signextend);	break;
		case 3: getX1(_dst, _signextend);	break;
		default:	assert(0 && "invalid QQ value");	break;
		}
	}

	void JitOps::decode_QQQQ_read(DspValue& _s1, bool _signextendS1, DspValue& _s2, bool _signextendS2, TWord _qqqq) const
	{
		switch (_qqqq)
		{
		case 0:  getX0(_s1, _signextendS1);	getX0(_s2, _signextendS2);	return;
		case 1:  getY0(_s1, _signextendS1);	getY0(_s2, _signextendS2);	return;
		case 2:  getX1(_s1, _signextendS1);	getX0(_s2, _signextendS2);	return;
		case 3:  getY1(_s1, _signextendS1);	getY0(_s2, _signextendS2);	return;
		case 4:  getX0(_s1, _signextendS1);	getY1(_s2, _signextendS2);	return;
		case 5:  getY0(_s1, _signextendS1);	getX0(_s2, _signextendS2);	return;
		case 6:  getX1(_s1, _signextendS1);	getY0(_s2, _signextendS2);	return;
		case 7:  getY1(_s1, _signextendS1);	getX1(_s2, _signextendS2);	return;
		case 8:  getX1(_s1, _signextendS1);	getX1(_s2, _signextendS2);	return;
		case 9:  getY1(_s1, _signextendS1);	getY1(_s2, _signextendS2);	return;
		case 10: getX0(_s1, _signextendS1);	getX1(_s2, _signextendS2);	return;
		case 11: getY0(_s1, _signextendS1);	getY1(_s2, _signextendS2);	return;
		case 12: getY1(_s1, _signextendS1);	getX0(_s2, _signextendS2);	return;
		case 13: getX0(_s1, _signextendS1);	getY0(_s2, _signextendS2);	return;
		case 14: getY0(_s1, _signextendS1);	getX1(_s2, _signextendS2);	return;
		case 15: getX1(_s1, _signextendS1);	getY1(_s2, _signextendS2);	return;

		default: assert(0 && "invalid QQQQ value");				return;
		}
	}

	void JitOps::decode_qqq_read(DspValue& _dst, const TWord _qqq) const
	{
		switch (_qqq)
		{
		case 2: getALU0(_dst, 0);			return;
		case 3: getALU0(_dst, 1);			return;
		case 4: getXY0(_dst, 0, false);		return;
		case 5: getXY0(_dst, 1, false);		return;
		case 6: getXY1(_dst, 0, false);		return;
		case 7: getXY1(_dst, 1, false);		return;
		default:
			assert(0 && "invalid qqq value");
			break;
		}
	}

	void JitOps::decode_sss_read(DspValue& _dst, const TWord _sss) const
	{
		switch( _sss )
		{
		case 2:		getALU1(_dst, 0);		break;
		case 3:		getALU1(_dst, 1);		break;
		case 4:		getXY0(_dst, 0, false);	break;
		case 5:		getXY0(_dst, 1, false);	break;
		case 6:		getXY1(_dst, 0, false);	break;
		case 7:		getXY1(_dst, 1, false);	break;
		case 0:
		case 1:
		default:
			assert( 0 && "invalid sss value" );
		}
	}
	void JitOps::decode_LLL_read(TWord _lll, DspValue& x, DspValue& y)
	{
		x.temp(DspValue::Temp24);
		y.temp(DspValue::Temp24);

		switch (_lll)
		{
		case 0:
		case 1:
		case 4:	// TODO: 48 bit saturation/limiting
		case 5:	// TODO: 48 bit saturation/limiting
			{
				const auto alu = _lll & 3;
				m_dspRegs.getALU(r64(y), alu);
				m_asm.mov(r64(x), r64(y));
				m_asm.shr(r64(x), asmjit::Imm(24));
				m_asm.and_(r32(x), asmjit::Imm(0xffffff));
				m_asm.and_(r32(y), asmjit::Imm(0xffffff));
			}
			break;
		case 2:
		case 3:
			{
				const auto xy = _lll - 2;
				m_dspRegs.getXY(r64(y), xy);
				m_asm.mov(r64(x), r64(y));
				m_asm.shr(r64(x), asmjit::Imm(24));
				m_asm.and_(r32(x), asmjit::Imm(0xffffff));
				m_asm.and_(r32(y), asmjit::Imm(0xffffff));
			}
			break;
		case 6:
			transferAluTo24(x, 0);
			transferAluTo24(y, 1);
			break;
		case 7:
			transferAluTo24(x, 1);
			transferAluTo24(y, 0);
			break;
		default:
			assert(0 && "invalid LLL value");
			break;
		}
	}

	void JitOps::decode_LLL_write(TWord _lll, const DspValue& x, const DspValue& y)
	{
		switch (_lll)
		{
		case 0:
		case 1:
			{
				const auto alu = _lll & 3;
				AluRef r(m_block, alu);
				m_asm.shr(r, asmjit::Imm(48));	// clear 48 LSBs
				m_asm.shl(r, asmjit::Imm(24));
				m_asm.or_(r, r64(x.get()));
				m_asm.shl(r, asmjit::Imm(24));
				m_asm.or_(r, r64(y.get()));
			}
			break;
		case 4:
		case 5:
			{
				const auto alu = _lll & 3;

				AluRef r(m_block, alu, false, true);

				m_asm.mov(r32(r), r32(x.get()));
				m_asm.shl(r64(r), asmjit::Imm(24));
				m_asm.or_(r64(r), r64(y.get()));

				signextend48to56(r);
			}
			break;
		case 2:
		case 3:
			{
				const auto xy = _lll - 2;

				const auto r = m_dspRegs.getXY(xy, JitDspRegs::Write);
				m_asm.mov(r32(r), r32(x.get()));
				m_asm.shl(r64(r), asmjit::Imm(24));
				m_asm.or_(r64(r), r64(y.get()));
			}
			break;
		case 6:
			transfer24ToAlu(0, x);
			transfer24ToAlu(1, y);
			break;
		case 7:
			transfer24ToAlu(1, x);
			transfer24ToAlu(0, y);
			break;
		default:
			assert(0 && "invalid LLL value");
			break;
		}
	}

	DspValue JitOps::decode_XMove_MMRRR(TWord _mm, TWord _rrr)
	{
		switch (_mm)
		{
		case 0: return updateAddressRegister(MMM_Rn, _rrr);
		case 1: return updateAddressRegister(MMM_RnPlusNn, _rrr);
		case 2: return updateAddressRegister(MMM_RnMinus, _rrr);
		case 3: return updateAddressRegister(MMM_RnPlus, _rrr);
		}
		assert(false && "invalid addressing mode");
		return DspValue(m_block);
	}
}
