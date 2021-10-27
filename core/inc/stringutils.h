#ifndef __LINKING_STRING_UTILS_H__
#define __LINKING_STRING_UTILS_H__

#include <iostream>
#include <string>
#include <memory>

namespace linking {

using namespace std;

class StringUtils {
public:

    template<typename ... Args>
    static string stringFormat(const string& format, Args ... args)
    {
        size_t size = 1 + snprintf(nullptr, 0, format.c_str(), args ...);
        char bytes[size];
        snprintf(bytes, size, format.c_str(), args ...);
        return string(bytes);
    }

    static void split(const string& s, vector<string>& tokens, const string& delimiters)
    {
        string::size_type lastPos = s.find_first_not_of(delimiters, 0);
        string::size_type pos = s.find_first_of(delimiters, lastPos);
        while(string::npos != pos || string::npos != lastPos) {
            tokens.push_back(s.substr(lastPos, pos - lastPos));
            lastPos = s.find_first_not_of(delimiters, pos);
            pos = s.find_first_of(delimiters, lastPos);
        }
    }
};

}

#endif
