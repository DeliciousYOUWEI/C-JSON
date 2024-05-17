// 这个宏定义是指：因为宏替换是发生在预处理阶段的，而可以选择控制是否要将以下段进行替换（因为如果重复引用的话，
// 当然现在没有，是指如果头文件涉及了include的话，避免两个文件重复引用，而选择是否要替换）。注意这个防范不是用来
// 在使用阶段也需要注意的，而是当控制了宏之后，哪怕一个文件里面两次发生include此文件，也可以通过（不使用宏会报错）
// 总之是头文件的话加上这个宏定义就完了。
#ifndef LEPTJSON_H__
#define LEPTJSON_H__
#include <stddef.h>


// 定义所有允许的类型。
typedef enum{LEPT_NULL, LEPT_TRUE, LEPT_FALSE, LEPT_STRING, LEPT_NUMBER, LEPT_ARRAY, LEPT_OBJECT} lept_type;

// 定义所有可能的函数返回类型。
typedef enum{LEPT_OK = 1, LEPT_INVALID_VALUE};

// 定义一个输出的结构体——这里不使用类的概念是因为类还可以封装行为，但是这个数据（jsonValue）不存在行为，因此用结构体即可
typedef struct lept_value lept_value;  // 想要将一个结构体的内部包含自身，就需要先定义这个结构体的名称，而后再在下文详细定义内容
typedef struct lept_obj_member lept_obj_member;


struct lept_value {
	union {
		double num;
		struct { size_t len; char* s; }str;
		struct { size_t elements_count; lept_value* a; }arr;
		struct { size_t members_count; lept_obj_member* m; }obj;
	}u;
	lept_type type;
};

struct lept_obj_value { 
	char* member_key;
	size_t key_len;
	lept_value* member_value;
};
  

int lept_prase(lept_value* output, const char* inJson);

lept_type lept_get_type(const lept_value* value); 

// 一个用于释放value中不定长度u（字符串、数组、对象）信息的函数。且用户在使用完prase的所有功能后也会需要使用该函数释放资源。
// 注意这个需要释放的资源依然指的是字符串、数组、对象，因为value_pointer指针在使用之后销毁其本身就可以释放数字与bool类型的资源，
// 只有不定长度的信息在该结构体中存储的是指针而非本体。
void lept_free(lept_value* value);

//用户操作一个布尔类型value的接口。
int lept_get_boolen(const lept_value* value);
void lept_set_boolen(lept_value* value, int b);

//用户操作一个number类型value的接口。
double lept_get_number(const lept_value* value);
void lept_set_number(lept_value* value, double num);

//用户操作一个string类型value的接口。
char* lept_get_string(const lept_value* value);
size_t lept_get_string_length(const lept_value* value);
void lept_set_string(lept_value* value, char* str, size_t str_len);

//用户操作一个array类型value的接口。
lept_value lept_get_array_value(lept_value* value, size_t index);
size_t lept_get_array_length(const lept_value* value);
void lept_set_arrray_value(lept_value* value, lept_value* target, size_t index);

//用户操作一个obj类型value的接口。（获取与设置对象的成员名，获取与设置对象的成员值）？都需要什么操作？


#endif