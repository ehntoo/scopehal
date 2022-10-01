#ifndef ScopehalUtils_h
#define ScopehalUtils_h

#include <string>
#include <vector>

#define FS_PER_SECOND 1e15
#define SECONDS_PER_FS 1e-15

extern std::vector<std::string> g_searchPaths;

float FreqToPhase(float hz);

uint64_t next_pow2(uint64_t v);
uint64_t prev_pow2(uint64_t v);

std::string Trim(const std::string& str);
std::string TrimQuotes(const std::string& str);
std::string BaseName(const std::string& path);

//string to size_t conversion
#ifdef _WIN32
#define stos(str) static_cast<size_t>(stoll(str))
#else
#define stos(str) static_cast<size_t>(stol(str))
#endif

std::string to_string_sci(double d);
std::string to_string_hex(uint64_t n, bool zeropad = false, int len = 0);

std::vector<std::string> explode(const std::string& str, char separator);
std::string str_replace(const std::string& search, const std::string& replace, const std::string& subject);

std::string ReadFile(const std::string& path);
std::string ReadDataFile(const std::string& relpath);
std::vector<uint32_t> ReadDataFileUint32(const std::string& relpath);
std::string FindDataFile(const std::string& relpath);
void GetTimestampOfFile(std::string path, time_t& timestamp, int64_t& fs);

#ifdef _WIN32
std::string NarrowPath(wchar_t* wide);
#else
std::string ExpandPath(const std::string& in);
void CreateDirectory(const std::string& path);
#endif

double GetTime();

std::string GetDefaultChannelColor(int i);

//Checksum helpers
uint32_t CRC32(const uint8_t* bytes, size_t start, size_t end);
uint32_t CRC32(const std::vector<uint8_t>& bytes);

#endif