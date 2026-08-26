#ifndef LV_STR_H
#define LV_STR_H
typedef enum {
    LANGUAGE_EN = 0,
    LANGUAGE_CN,
    LANGUAGE_KR
}language_t;

typedef enum {
    Str_reject,

    Str_Max
} str_id_t;
typedef struct {
    const char* name;
    const char* strEN; // 英语
    const char* strCN;  // 中文
    const char* strKR;  // 韩语

} STR_t;

const char* get_str_by_name(const char* name);
#endif // !LV_STR.H
