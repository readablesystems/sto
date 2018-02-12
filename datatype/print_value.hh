#pragma once
#include "compiler.hh"
#include <sstream>

namespace mass {

template <typename T> class is_printable : public false_type {};
template <> class is_printable<unsigned char> : public true_type {};
template <> class is_printable<signed char> : public true_type {};
template <> class is_printable<char> : public true_type {};
template <> class is_printable<bool> : public true_type {};
template <> class is_printable<unsigned short> : public true_type {};
template <> class is_printable<short> : public true_type {};
template <> class is_printable<unsigned> : public true_type {};
template <> class is_printable<int> : public true_type {};
template <> class is_printable<unsigned long> : public true_type {};
template <> class is_printable<long> : public true_type {};
template <> class is_printable<unsigned long long> : public true_type {};
template <> class is_printable<long long> : public true_type {};
template <> class is_printable<float> : public true_type {};
template <> class is_printable<double> : public true_type {};
template <> class is_printable<long double> : public true_type {};
template <typename C, typename T, typename A> class is_printable<std::basic_string<C, T, A> > : public true_type {};
template <> class is_printable<const char*> : public true_type {};
template <> class is_printable<char*> : public true_type {};

template <typename T, bool printable = is_printable<typename std::remove_cv<T>::type>::value > class print_value_helper;
template <typename T> class print_value_helper<T, true> {
public:
    typedef const T& type;
    static const T& print_value(const T& x) {
        return x;
    }
};
template <typename T> class print_value_helper<T, false> {
public:
    typedef std::string type;
    static std::string print_value(const T& x) {
        std::ostringstream buf;
        buf << "<&" << (void*) &x << ">";
        return buf.str();
    }
};

template <typename T>
auto print_value(const T& x) -> typename print_value_helper<T>::type {
    return print_value_helper<T>::print_value(x);
}

}
