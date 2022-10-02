#ifndef ScopehalColor_h
#define ScopehalColor_h

#include <cstdint>
#include <string>
#include <cstdio>

struct ScopehalColor
{
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	ScopehalColor()
		: red(0xFF)
		, green(0xFF)
		, blue(0xFF)
	{
	}

	ScopehalColor(unsigned char r, unsigned char g, unsigned char b)
		: red(r)
		, green(g)
		, blue(b)
	{
	}

	ScopehalColor(std::string str)
	{
		unsigned long long temp;
		sscanf(str.c_str(), "#%llx", &temp);
		blue = temp & 0xFF;
		green = (temp >> 8) & 0xFF;
		red = (temp >> 16) & 0xFF;
	}

	const std::string toString () const
	{
		char temp[8];
		snprintf(temp, sizeof(temp), "#%02x%02x%02x", red, green, blue);
		return temp;
	}

	void set_red(unsigned char r)
	{
		red = r;
	}

	void set_green(unsigned char g)
	{
		green = g;
	}

	void set_blue(unsigned char b)
	{
		blue = b;
	}

	unsigned char get_red(void) const
	{
		return red;
	}

	unsigned char get_green(void) const
	{
		return green;
	}

	unsigned char get_blue(void) const
	{
		return blue;
	}
};

#endif