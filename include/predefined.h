#ifndef PREDEFINED_H
#define PREDEFINED_H

#include <string.h>

namespace bpt {

/* predefined B+ info */
#define BP_ORDER 50


/* key/value type */
struct value_t{
    //数据类型
    char name[256];
    int age;
    char email[256];
};

// typedef int value_t;
struct key_t {
    char k[16];

    key_t(const char *str = "")
    {
        //bzero()将参数k 所指的内存区域前n 个字节全部设为零
        bzero(k, sizeof(k));
        strcpy(k, str);
    }
};

inline int keycmp(const key_t &a, const key_t &b) {
    //
    int x = strlen(a.k) - strlen(b.k);
    return x == 0 ? strcmp(a.k, b.k) : x;
}

//定义宏函数 OPERATOR_KEYCMP
#define OPERATOR_KEYCMP(type) \
    bool operator< (const key_t &l, const type &r) {\
        return keycmp(l, r.key) < 0;\
    }\
    bool operator< (const type &l, const key_t &r) {\
        return keycmp(l.key, r) < 0;\
    }\
    bool operator== (const key_t &l, const type &r) {\
        return keycmp(l, r.key) == 0;\
    }\
    bool operator== (const type &l, const key_t &r) {\
        return keycmp(l.key, r) == 0;\
    }
}
#endif /* end of PREDEFINED_H */
