// 这个宏定义是指：因为宏替换是发生在预处理阶段的，而可以选择控制是否要将以下段进行替换（因为如果重复引用的话，
// 当然现在没有，是指如果头文件涉及了include的话，避免两个文件重复引用，而选择是否要替换）。注意这个防范不是用来
// 在使用阶段也需要注意的，而是当控制了宏之后，哪怕一个文件里面两次发生include此文件，也可以通过（不使用宏会报错）
// 总之是头文件的话加上这个宏定义就完了。
#ifndef LEPTJSON_H__
#define LEPTJSON_H__
#include <stddef.h>

// 定义所有允许的类型。
typedef enum { LEPT_NULL, LEPT_TRUE, LEPT_FALSE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

// 定义所有可能的函数返回类型。
typedef enum { LEPT_OK = 1, LEPT_INVALID_VALUE, LEPT_OUT_OF_MEMORY, LEPT_SMALL_THAN_CARRENT_CAP };
# define LEPT_ELEMENT_NOT_EXIST ((size_t)-1)  // 获取不同设备上size_t最值的方式（注意size_t不能是负值，这里利用的就是让size_t溢出的方式获得最值）

// 定义一个输出的结构体——这里不使用类的概念是因为类还可以封装行为，但是这个数据（jsonValue）不存在行为，因此用结构体即可
typedef struct lept_value lept_value;  // 想要将一个结构体的内部包含自身，就需要先定义这个结构体的名称，而后再在下文详细定义内容
typedef struct lept_obj_member lept_obj_member;


struct lept_value {
	union {
		double num;
		struct { size_t len; char* s; }str;
		struct { size_t elements_count, capability; lept_value* a; }arr;
		struct { size_t members_count, capability; lept_obj_member* m; }obj;
	}u;
	lept_type type;
};

struct lept_obj_member {
	char* member_key;
	size_t key_len;
	lept_value member_value;
};


int lept_parse(lept_value* output, const char* inJson);

char* lept_generate(const lept_value* inValue, size_t* length);

lept_type lept_get_type(const lept_value* value);

// ====================================================指针操作======================================================
// 
// 一个用于释放value中不定长度u（字符串、数组、对象）信息的函数。且用户在使用完prase的所有功能后也会需要使用该函数释放资源。
// 注意这个需要释放的资源依然指的是字符串、数组、对象，因为value_pointer指针在使用之后销毁其本身就可以释放数字与bool类型的资源，
// 只有不定长度的信息在该结构体中存储的是指针而非本体。
void lept_free(lept_value* value);

void lept_copy(lept_value* trg_value, const lept_value* src_value);
void lept_move(lept_value* trg_value, lept_value* src_value);
void lept_swap(lept_value* trg_value, lept_value* src_value);
// ==================================================================================================================


// ===========================================用户对某个value的值操作接口===============================================
// 用户操作一个空类型的接口。
int lept_init_null(lept_value* value);


//用户操作一个布尔类型value的接口。
int lept_init_boolen(lept_value* value);  // 初始化默认为true
int lept_get_boolen(const lept_value* value);
void lept_set_boolen(lept_value* value, int b);

//用户操作一个number类型value的接口。
int lept_init_number(lept_value* value);  // 初始化默认为0
double lept_get_number(const lept_value* value);
void lept_set_number(lept_value* value, double num);

//用户操作一个string类型value的接口。
int lept_init_string(lept_value* value);  // 初始化字符串指针默认为null，长度为0
char* lept_get_string(const lept_value* value);
size_t lept_get_string_length(const lept_value* value);
void lept_set_string(lept_value* value, char* str, size_t str_len);

//用户操作一个array类型value的接口。
int lept_init_array(lept_value* value);  // 初始化数组指针默认为null，元素个数为0（注意init并不给value初始空间，初始空间为空，需要用户或解析器负责开辟）
int lept_set_array_capability(lept_value* value, size_t cap);  // 给一个动态数组分配空间，返回lept_ok或错误码
lept_value* lept_get_array_value(lept_value* value, size_t index);
size_t lept_get_array_elements_count(const lept_value* value);
size_t lept_get_array_capability(const lept_value* value);
size_t lept_set_array_value(lept_value* value, lept_value* target, size_t index);  // 正常设置完成返回LEPT_OK，否则返回LEPT_ELEMENT_NOT_EXIST（基于swap，因此输入value会变成索要替换的value）
int lept_append_array_elelment(lept_value* value, lept_value* target);  // 在动态列表的末尾插入一个元素，成功则返回ok，否则返回错误码
int lept_shrink_array(lept_value* value);  // 将动态数组的无用区间释放，返回新的数组头指针。该函数仅应在数组彻底修改完后使用。
lept_value* lept_pop_array_elelment(lept_value* value);  // 弹出最后一个value，并返回该value的指针（纯c并不支持函数重载，因此需要一个专门的函数带index）
int lept_insert_array(lept_value* value, lept_value* target, size_t index);  // 在array的指定位置插入一个值。（基于swap冒泡实现）
size_t lept_remove_array_element(lept_value* value, size_t index);


//用户操作一个obj类型value的接口。
int lept_init_obj(lept_value* value);  // 初始化对象成员数组指针默认为null，成员个数为0，该函数同样不分配初始空间
int lept_set_obj_capability(lept_value* value, size_t cap);  // 给一个动态数组分配空间，返回lept_ok或错误码
size_t lept_get_obj_members_count(lept_value* value);
const char* lept_get_obj_key(lept_value* value, size_t index);
size_t lept_get_obj_key_len(lept_value* value, size_t index);
lept_value* lept_get_obj_value(lept_value* value, size_t index);
size_t lept_find_obj_key(const lept_value* value, const char* target_key, size_t tklen);
lept_value* lept_find_obj_member(const lept_value* value, const char* target_key, size_t tklen);
int lept_is_equal(const lept_value* src_value, const lept_value* trg_value);
size_t lept_set_obj_value(lept_value* trg_obj, lept_obj_member* src_member);  // 返回LEPT_OK，否则返回LEPT_OBJ_KEY_NOT_EXIST（基于swap，因此输入value会变成索要替换的value）
int lept_append_obj_member(lept_value* value, const char* target_key, size_t klen, lept_value* src_member_value);
int lept_shrink_obj(lept_value* value);
size_t lept_remove_obj_member(lept_value* value, const char* target_key, size_t klen);

// =====================================================================================================================
#endif