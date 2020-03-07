#pragma once
#include <string>
#include <vector>
#include <fstream>
typedef unsigned int uint;
namespace StringUtil
{
	void IndicesOf(const std::string& str, const std::string& sign, std::vector<uint>& v);
	void IndicesOf(const std::string& str, char, std::vector<uint>& v);
	void CutToLine(const std::string& str, std::vector<std::string>& lines);
	void CutToLine(const char* str, int64_t size, std::vector<std::string>& lines);
	void ReadLines(std::ifstream& ifs, std::vector<std::string>& lines);
	int GetFirstIndexOf(const std::string& str, const std::string& sign);
	int GetFirstIndexOf(const std::string& str, char sign);
	void Split(const std::string& str, const std::string& sign, std::vector<std::string>& v);
	void Split(const std::string& str, char sign, std::vector<std::string>& v);
	void GetDataFromAttribute(const std::string& str, std::string& result);
	void GetDataFromBrackets(const std::string& str, std::string& result);
	int StringToInteger(const std::string& str);
	void ToLower(std::string& str);
	void ToUpper(std::string& str);
}