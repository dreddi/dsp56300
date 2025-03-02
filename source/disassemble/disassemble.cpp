#include <fstream>
#include <iostream>
#include <list>

#include "commandline.h"

#include "../dsp56kEmu/disasm.h"
#include "dsp56kEmu/memory.h"
#include "dsp56kEmu/peripherals.h"

using namespace dsp56k;

bool loadInputFile(std::vector<TWord>& _dst, const std::string& _filename, bool _bigEndian)
{
	std::ifstream file(_filename, std::ios::binary | std::ios::in);

	if (!file.is_open())
		return false;

	file.seekg(0, std::ios::end);
	const auto fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	if (!fileSize)
	{
		std::cout << "File " << _filename << " is empty" << std::endl;
		return false;
	}

	std::vector<uint8_t> buffer;
	buffer.resize(fileSize);

	file.read(reinterpret_cast<char*>(&buffer.front()), fileSize);
	file.close();

	std::list<uint8_t> data;
	data.insert(data.begin(), buffer.begin(), buffer.end());

	// ASCII?
	bool isBinary = false;

	for (auto byte : data)
	{
		const bool ascii = ((byte >= '0' && byte <= '9') ||
			(byte >= 'A' && byte <= 'F') ||
			(byte >= 'a' && byte <= 'f') ||
			byte == ' ' || byte == '\n' || byte == '\r' || byte == '\t');

		if (!ascii)
		{
			isBinary = true;
			break;
		}
	}

	if(!isBinary)
	{
		std::cout << "Detected input file as ASCII" << std::endl;

		for(auto it = data.begin(); it != data.end();)
		{
			switch (*it)
			{
			case '\n':
			case '\r':
			case ' ':
				it = data.erase(it);
				break;
			default:
				if (*it >= 'A' && *it <= 'F')
				{
					data.insert(it, (*it) + 'a' - 'A');
					it = data.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		if(data.size() / 6 * 6 != data.size())
		{
			std::cout << "ASCII input length must be a multiple of 6 (two characters for each byte, six characters build one DSP word)" << std::endl;
			return false;
		}

		const auto toNibble = [](char _c)
		{
			if (_c >= '0' && _c <= '9')
				return _c - '0';
			assert(_c >= 'a' && _c <= 'f');
			return _c - 'a' + 0xa;
		};

		buffer.resize(data.size() >> 1);
		auto it = data.begin();
		for (auto& i : buffer)
		{
			auto byte = toNibble(*it++) << 4;
			byte |= toNibble(*it++);
			i = static_cast<uint8_t>(byte);
		}
	}
	else
	{
		std::cout << "Detected input file as binary" << std::endl;
	}

	if(buffer.size() / 3 * 3 != buffer.size())
	{
		std::cout << "Binary input length must be a multiple of 6 (three bytes build one DSP word)" << std::endl;
		return false;
	}

	_dst.resize(buffer.size() / 3);

	size_t s = 0;

	for (TWord& d : _dst)
	{
		const TWord a = buffer[s++];
		const TWord b = buffer[s++];
		const TWord c = buffer[s++];

		if (_bigEndian) d = (a << 16) | (b << 8) | c;
		else			d = (c << 16) | (b << 8) | a;
	}

	return true;
}

int main(int _argc, char* _argv[])
{
	try
	{
		const CommandLine cmd(_argc, _argv);

		if (!cmd.contains("in"))
		{
			std::cout << "Motorola DSP 56300 Disassembler" << std::endl;
			std::cout << std::endl;
			std::cout << "Usage:" << std::endl;
			std::cout << std::endl;
			std::cout << "disassemble -in inputfile [-out outputfile]" << std::endl;
			std::cout << std::endl;
			std::cout << "";
			std::cout << std::endl;
			std::cout << "Options:" << std::endl;
			std::cout << "-in filename     Input file, required. Input may either be a file with ASCII text in form 0055ff aabbcc FF0022 ... or binary content." << std::endl;
			std::cout << "-out filename    Write output to a text file. May be omitted, in which case output is written to standard output." << std::endl;
			std::cout << "-s filename      Specify file with additional symbols in format S:abcdef=name, where S = X, Y, L, P or I." << std::endl;
			std::cout << "-pc aabbcc       Set base address in hex. Defaults to 0 if omitted." << std::endl;
			std::cout << "le               Specify that the input file is in Little-Endian format. By default, input bytes are treated as being Big-Endian." << std::endl;
			std::cout << std::endl;
			std::cout << "Output format:" << std::endl;
			std::cout << "[address] - [opcode word A] [opcode word B] = [assembly]" << std::endl;
			return -1;
		}

		const auto inFile = cmd.get("in");

		const auto outFile = cmd.contains("out") ? cmd.get("out") : std::string();

		const auto symbolFile = cmd.contains("s") ? cmd.get("s") : std::string();

		std::unique_ptr<std::ofstream> outf;

		if(!outFile.empty())
		{
			outf.reset(new std::ofstream(outFile, std::ios::out));

			if(!outf->is_open())
			{
				std::cout << "Failed to create output file " << outFile << std::endl;
				return -1;
			}
		}

		std::basic_ostream<char>& out = outf ? *outf : std::cout;

		const auto bigEndian = !cmd.contains("le");

		std::vector<TWord> input;
		if (!loadInputFile(input, inFile, bigEndian))
		{
			std::cout << "Failed to load input file " << inFile << std::endl;
			return -1;
		}

		TWord pc = 0;
		if(cmd.contains("pc"))
		{
			const auto pcStr = cmd.get("pc");
			pc = strtol(pcStr.c_str(), nullptr, 16);
		}

		Opcodes opcodes;
		Disassembler disasm(opcodes);
		Peripherals56362 p;
		p.setSymbols(disasm);	
		Peripherals56367 pY;
		pY.setSymbols(disasm);	

		if(!symbolFile.empty())
		{
			std::ifstream s(symbolFile.c_str(), std::ios::in);
			if(!s.is_open())
			{
				std::cout << "Failed to open symbol file " << symbolFile << std::endl;
				return -1;
			}

			std::string line;
			while(std::getline(s, line))
			{
				if(line.empty())
					continue;

				if(line.find_first_of(" \t/;") == 0)
					continue;

				const auto splitA = line.find(':');
				const auto splitB = line.find('=');

				if(splitA == std::string::npos || splitB == std::string::npos)
				{
					LOG("Symbol has wrong format, needs to be S:address=name. S = X, Y, L, P or I, address in hex");
					return -1;
				}

				const auto areaStr = line.substr(0, splitA);
				const auto addressStr = line.substr(splitA + 1, splitB - splitA - 1);
				
				auto valueStr = line.substr(splitB + 1);
				
				const auto splitCommentPos = valueStr.find_first_of(" \t/;");
				if (splitCommentPos != std::string::npos)
					valueStr = valueStr.substr(0, splitCommentPos);

				const auto addr = strtol(addressStr.c_str(), nullptr, 16);

				Disassembler::SymbolType type;
				switch (areaStr[0])
				{
				case 'X':
				case 'x':	type = Disassembler::MemX;	break;
				case 'Y':
				case 'y':	type = Disassembler::MemY;	break;
				case 'P':
				case 'p':	type = Disassembler::MemP;	break;
				case 'l':
				case 'L':	type = Disassembler::MemL;	break;
				case 'i':
				case 'I':	type = Disassembler::Immediate;	break;
				default:
					LOG("Unknown symbol type " << areaStr << ", needs to be X, Y, P, L or I");
					return -1;
				}

				disasm.addSymbol(type, addr, valueStr);
			}
		}

		std::string assembly;
		disasm.disassembleMemoryBlock(assembly, input, pc, true, true, true);

		out << assembly;

		return 0;
	}
	catch (const std::runtime_error& e)
	{
		std::cout << "Fatal error: " << e.what();
		return -1;
	}
	catch (const std::exception& e)
	{
		std::cout << "Fatal error: " << e.what();
		return -1;
	}
}
