#ifndef XBOX_IOSTREAM_INJECTOR_HPP
#define XBOX_IOSTREAM_INJECTOR_HPP

#ifdef __cplusplus
#include <string>
namespace std {
    class XboxDummyStream {
    public:
        template <typename T>
        XboxDummyStream& operator<<(const T& val) { return *this; }
        XboxDummyStream& operator<<(XboxDummyStream& (*)(XboxDummyStream&)) { return *this; }
    };
    inline XboxDummyStream cout;
    inline XboxDummyStream cin;
    inline XboxDummyStream& endl(XboxDummyStream& os) { return os; }
    static inline XboxDummyStream& getline(XboxDummyStream& is, std::string& str) {
        str = "";
        return is;
    }
}
#endif

#endif
