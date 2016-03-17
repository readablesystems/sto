#pragma once
#include <string>
#include "Packer.hh"

class StringWrapper {
public:
    StringWrapper(std::string& s)
        : sptr_(&s) {
    }
    StringWrapper(const std::string& s)
        : sptr_(const_cast<std::string*>(&s)) {
    }
    std::string* value() const {
        return sptr_;
    }
    operator std::string&() {
        return *sptr_;
    }
    operator const std::string&() const {
        return *sptr_;
    }
private:
    std::string* sptr_;
};

template <>
struct Packer<std::string, false> {
    template <typename... Args>
    static void* pack(TransactionBuffer& buf, Args&&... args) {
        return buf.template allocate<std::string>(std::forward<Args>(args)...);
    }
    static void* pack(TransactionBuffer&, const StringWrapper& wrapper) {
        return wrapper.value();
    }
    static void* pack(TransactionBuffer&, StringWrapper&& wrapper) {
        return wrapper.value();
    }
    static void* pack_unique(TransactionBuffer& buf, const std::string& x) {
        if (const void* ptr = buf.find<UniqueKey<std::string> >(x))
            return const_cast<void*>(ptr);
        else
            return buf.allocate<UniqueKey<std::string> >(x);
    }
    template <typename... Args>
    static void* repack(TransactionBuffer& buf, void*, Args&&... args) {
        return pack(buf, std::forward<Args>(args)...);
    }
    static std::string& unpack(void* x) {
        return *(std::string*) x;
    }
};
