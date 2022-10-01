#include "ScopehalUtils.h"
#include <wordexp.h>
#include "log.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

using namespace std;

vector<string> g_searchPaths;

/**
	@brief Converts a frequency in Hz to a phase velocity in rad/sec
 */
float FreqToPhase(float hz)
{
	return 2 * M_PI * hz;
}

/**
	@brief Rounds a 64-bit integer up to the next power of 2
 */
uint64_t next_pow2(uint64_t v)
{
#ifdef __GNUC__
	if(v == 1)
		return 1;
	else
		return 1 << (64 - __builtin_clzl(v-1));
#else
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	v++;
	return v;
#endif
}

/**
	@brief Rounds a 64-bit integer down to the next power of 2
 */
uint64_t prev_pow2(uint64_t v)
{
	uint64_t next = next_pow2(v);

	if(next == v)
		return v;
	else
		return next/2;
}

/**
	@brief Removes whitespace from the start and end of a string
 */
string Trim(const string& str)
{
	string ret;
	string tmp;

	//Skip leading spaces
	size_t i=0;
	for(; i<str.length() && isspace(str[i]); i++)
	{}

	//Read non-space stuff
	for(; i<str.length(); i++)
	{
		//Non-space
		char c = str[i];
		if(!isspace(c))
		{
			ret = ret + tmp + c;
			tmp = "";
		}

		//Space. Save it, only append if we have non-space after
		else
			tmp += c;
	}

	return ret;
}

/**
	@brief Removes quotes from the start and end of a string
 */
string TrimQuotes(const string& str)
{
	string ret;
	string tmp;

	//Skip leading spaces
	size_t i=0;
	for(; i<str.length() && (str[i] == '\"'); i++)
	{}

	//Read non-space stuff
	for(; i<str.length(); i++)
	{
		//Non-quote
		char c = str[i];
		if(c != '\"')
		{
			ret = ret + tmp + c;
			tmp = "";
		}

		//Quote. Save it, only append if we have non-quote after
		else
			tmp += c;
	}

	return ret;
}

string BaseName(const string & path)
{
	return path.substr(path.find_last_of("/\\") + 1);
}

/**
	@brief Like std::to_string, but output in scientific notation
 */
string to_string_sci(double d)
{
	char tmp[32];
	snprintf(tmp, sizeof(tmp), "%e", d);
	return tmp;
}

/**
	@brief Like std::to_string, but output in hex
 */
string to_string_hex(uint64_t n, bool zeropad, int len)
{
	char format[32];
	if(zeropad)
		snprintf(format, sizeof(format), "%%0%dlx", len);
	else if(len > 0)
		snprintf(format, sizeof(format), "%%%dlx", len);
	else
		snprintf(format, sizeof(format), "%%lx");

	char tmp[32];
	snprintf(tmp, sizeof(tmp), format, n);
	return tmp;
}

/**
	@brief Splits a string up into an array separated by delimiters
 */
vector<string> explode(const string& str, char separator)
{
	vector<string> ret;
	string tmp;
	for(auto c : str)
	{
		if(c == separator)
		{
			if(!tmp.empty())
				ret.push_back(tmp);
			tmp = "";
		}
		else
			tmp += c;
	}
	if(!tmp.empty())
		ret.push_back(tmp);
	return ret;
}

/**
	@brief Replaces all occurrences of the search string with "replace" in the given string
 */
string str_replace(const string& search, const string& replace, const string& subject)
{
	string ret;

	//This can probably be made more efficient, but for now we only call it on very short strings
	for(size_t i=0; i<subject.length(); i++)
	{
		//Match?
		if(0 == strncmp(&subject[i], &search[0], search.length()))
		{
			ret += replace;
			i += search.length() - 1;
		}

		//No, just copy
		else
			ret += subject[i];
	}

	return ret;
}

/**
	@brief Returns the contents of a file
 */
string ReadFile(const string& path)
{
	//Read the file
	FILE* fp = fopen(path.c_str(), "rb");
	if(!fp)
	{
		LogWarning("ReadFile: Could not open file \"%s\"\n", path.c_str());
		return "";
	}
	fseek(fp, 0, SEEK_END);
	size_t fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char* buf = new char[fsize + 1];
	if(fsize != fread(buf, 1, fsize, fp))
	{
		LogWarning("ReadFile: Could not read file \"%s\"\n", path.c_str());
		delete[] buf;
		fclose(fp);
		return "";
	}
	buf[fsize] = 0;
	fclose(fp);

	string ret(buf, fsize);
	delete[] buf;

	return ret;
}

/**
	@brief Locates and returns the contents of a data file as a std::string
 */
string ReadDataFile(const string& relpath)
{
	FILE* fp = NULL;
	for(auto dir : g_searchPaths)
	{
		string path = dir + "/" + relpath;
		fp = fopen(path.c_str(), "rb");
		if(fp)
			break;
	}

	if(!fp)
	{
		LogWarning("ReadDataFile: Could not open file \"%s\"\n", relpath.c_str());
		return "";
	}
	fseek(fp, 0, SEEK_END);
	size_t fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char* buf = new char[fsize + 1];
	if(fsize != fread(buf, 1, fsize, fp))
	{
		LogWarning("ReadDataFile: Could not read file \"%s\"\n", relpath.c_str());
		delete[] buf;
		fclose(fp);
		return "";
	}
	buf[fsize] = 0;
	fclose(fp);

	string ret(buf, fsize);
	delete[] buf;

	return ret;
}

/**
	@brief Locates and returns the contents of a data file as a std::vector<uint32_t>
 */
vector<uint32_t> ReadDataFileUint32(const string& relpath)
{
	vector<uint32_t> buf;

	FILE* fp = NULL;
	for(auto dir : g_searchPaths)
	{
		string path = dir + "/" + relpath;
		fp = fopen(path.c_str(), "rb");
		if(fp)
			break;
	}

	if(!fp)
	{
		LogWarning("ReadDataFile: Could not open file \"%s\"\n", relpath.c_str());
		return buf;
	}
	fseek(fp, 0, SEEK_END);
	size_t fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	size_t wordsize = fsize / sizeof(uint32_t);
	buf.resize(wordsize);
	if(wordsize != fread(&buf[0], sizeof(uint32_t), wordsize, fp))
	{
		LogWarning("ReadDataFile: Could not read file \"%s\"\n", relpath.c_str());
		fclose(fp);
		return buf;
	}
	fclose(fp);

	return buf;
}

/**
	@brief Locates a data file
 */
string FindDataFile(const string& relpath)
{
	for(auto dir : g_searchPaths)
	{
		string path = dir + "/" + relpath;
		FILE* fp = fopen(path.c_str(), "rb");
		if(fp)
		{
			fclose(fp);
			return path;
		}
	}

	return "";
}

void GetTimestampOfFile(string path, time_t& timestamp, int64_t& fs)
{
	//TODO: Add Windows equivalent
	#ifndef _WIN32
		struct stat st;
		if(0 == stat(path.c_str(), &st))
		{
			timestamp = st.st_mtim.tv_sec;
			fs = st.st_mtim.tv_nsec * 1000L * 1000L;
		}
	#endif
}

#ifdef _WIN32

string NarrowPath(wchar_t* wide)
{
	char narrow[MAX_PATH];
	const auto len = wcstombs(narrow, wide, MAX_PATH);

	if(len == static_cast<size_t>(-1))
		throw runtime_error("Failed to convert wide string");

	return std::string(narrow);
}
#endif

#ifndef _WIN32
// POSIX-specific filesystem helpers. These will be moved to xptools in a generalized form later.

// Expand things like ~ in path
string ExpandPath(const string& in)
{
	wordexp_t result;
	wordexp(in.c_str(), &result, 0);
	auto expanded = result.we_wordv[0];
	string out{ expanded };
	wordfree(&result);
	return out;
}

void CreateDirectory(const string& path)
{
	const auto expanded = ExpandPath(path);

	struct stat fst{ };

	// Check if it exists
	if(stat(expanded.c_str(), &fst) != 0)
	{
		// If not, create it
		if(mkdir(expanded.c_str(), 0755) != 0 && errno != EEXIST)
		{
			perror("");
			throw runtime_error("failed to create preferences directory");
		}
	}
	else if(!S_ISDIR(fst.st_mode))
	{
		// Exists, but is not a directory
		throw runtime_error("preferences directory exists but is not a directory");
	}
}
#endif

double GetTime()
{
	timespec t;
	clock_gettime(CLOCK_REALTIME,&t);
	double d = static_cast<double>(t.tv_nsec) / 1E9f;
	d += t.tv_sec;
	return d;
}

string GetDefaultChannelColor(int i)
{
	const int NUM_COLORS = 12;
	static const char* colorTable[NUM_COLORS] =
	{
		// cppcheck-suppress constStatement
		"#a6cee3",
		"#1f78b4",
		"#b2df8a",
		"#33a02c",
		"#fb9a99",
		"#e31a1c",
		"#fdbf6f",
		"#ff7f00",
		"#cab2d6",
		"#6a3d9a",
		"#ffff99",
		"#b15928"
	};

	return colorTable[i % NUM_COLORS];
}

/**
	@brief Calculates a CRC32 checksum using the standard Ethernet polynomial
 */
uint32_t CRC32(const uint8_t* bytes, size_t start, size_t end)
{
	uint32_t poly = 0xedb88320;

	uint32_t crc = 0xffffffff;
	for(size_t n=start; n <= end; n++)
	{
		uint8_t d = bytes[n];
		for(int i=0; i<8; i++)
		{
			bool b = ( crc ^ (d >> i) ) & 1;
			crc >>= 1;
			if(b)
				crc ^= poly;
		}
	}

	return ~(	((crc & 0x000000ff) << 24) |
				((crc & 0x0000ff00) << 8) |
				((crc & 0x00ff0000) >> 8) |
				 (crc >> 24) );
}

uint32_t CRC32(const vector<uint8_t>& bytes)
{
	return CRC32(&bytes[0], 0, bytes.size()-1);
}
