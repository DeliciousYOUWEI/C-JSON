#include "leptjson.h"
#include <assert.h>
#include <stdlib.h>
#include<malloc.h>
#include<string.h>
#include<stdio.h>

// 先定义一个context对象，用来后面来回传，基本上所有函数的传参都由这个context负责。
// 将栈定义在这个结构体里面是为了栈复用，而所有地方压入栈的方式都是一样的，都是操作栈顶指针，唯一区别的地方就是
// 需要在刚拿到栈的时候记录一下栈顶指针的位置，回头退出的时候把指针退到这个位置上（栈顶指针复位）
typedef struct {
	char* json;
	char* stack_point;  // 定义栈的首地址
	size_t top;  // 定义栈顶指针的位置（栈顶指针指向的就是下一个要赋值操作的地址）
	size_t cur_len;  // 定义指针所指向的内存总长
}json_context;

// 我不打算制作init这种可能会由于误操作导致内存泄露的函数，因此直接将该函数定义为不可被获取的宏定义
#define lept_init(value) do {\
	switch (value->type) {\
		case(LEPT_NUMBER): {\
			lept_init_number(value);\
			break;\
		}\
		case(LEPT_STRING): {\
			lept_init_string(value);\
			break;\
		}\
		case(LEPT_ARRAY): {\
			lept_init_array(value);\
			break;\
		}\
		case(LEPT_OBJECT): {\
			lept_init_obj(value);\
			break;\
	}}}while(0)


// ====================================================指针操作======================================================
// 清空一个现有的value结构体下的内存（如果不具有下属内存就无需清理）
void lept_free(lept_value* value) {

	// 如果类型是三个不定长类，就释放内存，且重置相关变量。
	switch (value->type) {
	case(LEPT_STRING): {
		value->u.str.len = 0;
		free(value->u.str.s);  // 注意free不能free一个野指针，至少需要是NULL才能被free，且这个指针不能是随便一个地址，必须要是一个正常分配出来的首地址，从首地址往后运算出来的地址拿来free是不行的
		// 这个指针下是存在总长的，总长里面就是全部的value结构体长度（而非只有指针的长度)（但是其中的字符串头、子数列等都没有被释放）
		return;
	}
	case(LEPT_ARRAY): {
		int temp_index;
		for (temp_index = 0; temp_index < value->u.arr.elements_count; temp_index++) {
			lept_free(value->u.arr.a + temp_index);
		}
		free(value->u.arr.a);  // 这个是释放列表本身所占的内存，上面的循环是释放掉列表中的指针指向的那些内存
		value->u.arr.elements_count = 0;
		return;
	}
	case(LEPT_OBJECT): {
		int temp_index;
		for (temp_index = 0; temp_index < value->u.obj.members_count; temp_index++) {
			free((value->u.obj.m + temp_index)->member_key);
			lept_free(&(value->u.obj.m + temp_index)->member_value);  // 这里因为结构体里面放的是本体不是指针，因此不用再释放一遍本体了，等下面释放结构体列表的时候会一并释放掉
		}
		free(value->u.obj.m);
		value->u.obj.members_count = 0;
		return;
	}
	default:return;
	}
}

// 深度复制一个value到另一个value（将所有下属内存的信息都复制，完全避免仅赋值指针）
void lept_copy(lept_value* trg_value, const lept_value* src_value) {
	lept_free(trg_value);  // 首先清空目标value
	trg_value->type = src_value->type;
	switch (src_value->type) {
	case(LEPT_NULL || LEPT_TRUE || LEPT_FALSE): {
		return;  // 三种无内部值的直接返回就行
	}
	case(LEPT_NUMBER): {
		trg_value->u.num = src_value->u.num;
		return;
	}
	case(LEPT_STRING): {
		trg_value->u.str.len = src_value->u.str.len;
		trg_value->u.str.s = (char*)malloc(src_value->u.str.len + 1);
		memcpy(trg_value->u.str.s, src_value->u.str.s, src_value->u.str.len + 1);
		return;
	}
	case(LEPT_ARRAY): {
		trg_value->u.arr.elements_count = src_value->u.arr.elements_count;
		trg_value->u.arr.a = (lept_value*)malloc(sizeof(lept_value) * src_value->u.arr.elements_count);
		int temp_index;
		for (temp_index = 0; temp_index < src_value->u.arr.elements_count; temp_index++) {
			lept_copy(trg_value->u.arr.a + temp_index, src_value->u.arr.a + temp_index);
		}
		return;
	}
	case(LEPT_OBJECT): {
		trg_value->u.obj.members_count = src_value->u.obj.members_count;
		trg_value->u.obj.m = (lept_obj_member*)malloc(sizeof(lept_obj_member) * src_value->u.obj.members_count);
		int temp_index;
		for (temp_index = 0; temp_index < src_value->u.obj.members_count; temp_index++) {
			(trg_value->u.obj.m + temp_index)->key_len = (src_value->u.obj.m + temp_index)->key_len;
			(trg_value->u.obj.m + temp_index)->member_key = (char*)malloc((src_value->u.obj.m + temp_index)->key_len + 1);
			memcpy((trg_value->u.obj.m + temp_index)->member_key, (src_value->u.obj.m + temp_index)->member_key, (src_value->u.obj.m + temp_index)->key_len + 1);

			lept_copy(&(trg_value->u.obj.m + temp_index)->member_value, &(src_value->u.obj.m + temp_index)->member_value);
		}
		return;
	}
	}
}

// move和copy不同，是将指针的所有权更换到新的value下，而旧的value内部的下属指针均置为NULL（注意实际上value本身的下属指针只有lept_value*和lept_obj_member*）
// 因此move是不需要递归的，只需要移交最上级指针的所有权即可
void lept_move(lept_value* trg_value, lept_value* src_value) {

	// 实质上移交所有权就是直接让trg拥有src的所有值，然后清空src下的指针
	lept_free(trg_value);
	trg_value->type = src_value->type;
	memcpy(trg_value, src_value, sizeof(lept_value));

	lept_init(src_value);
	return;
}

// 交换两个value的值
void lept_swap(lept_value* trg_value, lept_value* src_value) {
	lept_value temp_value;
	temp_value = *src_value;
	*src_value = *trg_value;
	*trg_value = temp_value;
}
// ==================================================================================================================

// =====================================================栈操作=======================================================
// 想把结构体传为参数，如果没有typedef，那就要写成struct XX*，如果使用了typedef，则直接用XX*就行了。
// 该函数用来返回栈里面当前可以用于插入的指针返回的类型是void类型的指针，可以自行转化成要转存的类型。
static void* get_point(json_context* context, size_t ex_size) {

	// 如果还是个新创建的栈，就需要给栈初始化长度，同时将cur_len赋值，并返回当前的point位置
	if (context->cur_len == 0) {
		context->stack_point = (char*)malloc(256);
		context->cur_len = 256;
		context->top += ex_size;
		return context->stack_point;
	}

	// 如果不是新创建的栈，就查看当前要操作的地址是否还在所申请的长度中
	else {
		// 还在申请范围内，则返回当前待操作地址，并将栈顶指针增加
		if (context->top < context->cur_len) {
			size_t move = context->top;
			context->top += ex_size;
			return context->stack_point + move;
		}
		// 如果不在申请范围内了，则申请扩大内存（扩增倍率1.5）
		else {
			context->cur_len += (context->cur_len >> 1);
			context->stack_point = (char*)realloc(context->stack_point, context->cur_len);  // 《这里存在内存泄漏风险，realloc直接原指针赋值了》
			size_t move = context->top;
			context->top += ex_size;
			return context->stack_point + move;
		}
	}
}
// ===================================================================================================================

// ===================================================解析器部分======================================================
static void lept_parse_whitespace(json_context* context) {
	char* real_json_word = context->json;
	// 注意所有的转义字符，其本质上在计算机内都是一个字符！
	while (*real_json_word == ' ' || *real_json_word == '\t' || *real_json_word == '\r' || *real_json_word == '\n') {
		real_json_word++;
	}
	context->json = real_json_word;
}

static int lept_parse_null(lept_value* output, json_context* context) {
	// 注意从指针里面直接偏移(偏移运算符本身带取值功能)，就自动会取值了，不需要再加*
	if (context->json[0] != 'n' || context->json[1] != 'u' || context->json[2] != 'l' || context->json[3] != 'l') {
		return LEPT_INVALID_VALUE;
	}
	output->type = LEPT_NULL;
	context->json += 4;
	return LEPT_OK;
}

static int lept_parse_true(lept_value* output, json_context* context) {
	if (context->json[0] != 't' || context->json[1] != 'r' || context->json[2] != 'u' || context->json[3] != 'e') {
		return LEPT_INVALID_VALUE;
	}
	output->type = LEPT_TRUE;
	context->json += 4;
	return LEPT_OK;
}

static int lept_parse_false(lept_value* output, json_context* context) {
	if (context->json[0] != 'f' || context->json[1] != 'a' || context->json[2] != 'l' || context->json[3] != 's' || context->json[4] != 'e') {
		return LEPT_INVALID_VALUE;
	}
	output->type = LEPT_FALSE;
	context->json += 5;
	return LEPT_OK;
}

static int lept_parse_number(lept_value* output, json_context* context) {
	char* parsed_char;
	output->u.num = strtod(context->json, &parsed_char);
	if (parsed_char == context->json) {
		return LEPT_INVALID_VALUE;
	}
	context->json = parsed_char;
	output->type = LEPT_NUMBER;
	return LEPT_OK;
}

static int lept_parse_string(lept_value* value, json_context* context) {
	// 保存当前的栈顶指针位置
	size_t cur_top = context->top;

	//前移一位去掉最前面的双引号。
	context->json++;

	// 死循环，只要没到下一个\"就算还没结束当前字符串，注意，是不需要判别转义字符的，因为无论如何转义，都会存在显式的"
	while (1) {
		switch (*context->json) {
		case '\"':
			//如果到了下一个\"了，这个双引号就不用往里面放了，直接开始将栈中的信息转存进用户的目标结构体中，然后释放资源退出
			context->json++;  // 跳过最后的一个双引号

			lept_init_string(value);
			lept_set_string(value, context->stack_point + cur_top, context->top - cur_top);
			context->top = cur_top;  // 指针的释放不能再放在这里（因为这里已经不再是栈的彻底弃用的地方了），但这里需要将栈返回成函数刚拿到时候的状态。
			return LEPT_OK;

		default:
			*(char*)get_point(context, sizeof(*context->json)) = *context->json;
			context->json++;
			continue;
		}
	}
}

static int lept_parse_array(lept_value* value, json_context* context) {
	// 保存一份当前的栈顶
	size_t cur_top = context->top;

	// 偏移指针拿掉最开头的[
	context->json++;
	lept_parse_whitespace(context);  // 去一下可能存在在符号之后的空格。

	lept_value temp_value;  // 创建一个临时的value，这个value就是将要保存一会嵌套parse并且最终压进栈的value。
	size_t temp_len = 0;

	// 死循环，只要没到下一个左括号，就说明当前array还没有结束，循环还需要继续
	while (1) {
		int ret = lept_parse_value(&temp_value, context);  // 把当前的这个元素解析出来存放在临时的temp_value中
		// 如果成功解析了，则进入保存阶段
		if (ret == LEPT_OK) {
			*(lept_value*)get_point(context, sizeof(temp_value)) = temp_value;  // 把解析出来的临时value压入栈中（注意这个操作是将“指针指向的那段区域的指针”的“值”赋值为了一个既有结构体。本质上是“A结构体”=“B结构体”，即使用一个结构体来赋值另一个结构体，这个并不是所有数据类型都支持这么操作，至少字符串不行）
			temp_len++;  // 只要成功解析了一个元素，就将当前数组的临时长度+1

			switch (*context->json) {
			case(','):
				context->json++;
				lept_parse_whitespace(context);  // 把可能存在的空白解析掉

				continue;  // 如果是逗号，就把这个逗号跳过，直接进行下一轮（下一轮里面会自己解析掉空格）
			case(']'):
				context->json++;
				lept_parse_whitespace(context);  // 处理当前json的结尾空格与】符
				lept_init_array(value);
				int ret = lept_set_array_capability(value, temp_len);
				if (ret == LEPT_OK) {
					value->u.arr.elements_count = temp_len;
					memcpy(value->u.arr.a, context->stack_point + cur_top, sizeof(temp_value) * temp_len);  // 将栈中的东西复制进结果中
					context->top = cur_top;  // 复位栈顶指针
					return LEPT_OK;
				}
				return ret;
			}
		}
		else {
			return LEPT_INVALID_VALUE;
		}
	}

}

static int lept_parse_object(lept_value* value, json_context* context) {
	// 该解析函数同样需要公用栈，因此保存栈顶指针位置
	size_t cur_top = context->cur_len;

	// 定义并初始化一个临时的对象成员
	lept_obj_member temp_member;
	temp_member.member_key = NULL;
	temp_member.key_len = 0;

	// 定义一个临时value用于保存key的字符串信息。
	lept_value temp_key;

	// 定义返回值保存变量
	int ret;

	size_t temp_len = 0;

	// 跳过起始的{并解析掉可能存在的空格，并进一步判断是否下一个字符是\"，如果不是则退出
	context->json++;
	lept_parse_whitespace(context);
	if (*context->json == '\"') {
		while (1) {
			// 解析key
			ret = lept_parse_string(&temp_key, context);
			if (ret == LEPT_OK) {
				lept_parse_whitespace(context);
				if (*context->json == ':') {
					context->json++;  // 跳过这个冒号
					lept_parse_whitespace(context);  // 再次解析掉可能存在的空格
					ret = lept_parse_value(&temp_member.member_value, context);
					if (ret == LEPT_OK) {
						// 如果值也解析成功，则该成员解析完毕，开始保存环节
						// temp_key的指针直接赋值给temp_member即可，因为parse-string的过程中会将指针先置为NULL（即保护起原有的地址）而后再free（这个free是给set功能用的）
						temp_member.member_key = temp_key.u.str.s;  // 将key赋值
						temp_member.key_len = temp_key.u.str.len;  // 将字符串长度赋值

						*(lept_obj_member*)get_point(context, sizeof(lept_obj_member)) = temp_member;  // 将这个member压栈

						temp_len++;

						switch (*context->json)
						{
						case(','):
							context->json++;
							lept_parse_whitespace(context);
							continue;
						case('}'):
							context->json++;

							lept_init_obj(value);
							value->u.obj.members_count = temp_len;
							lept_set_obj_capability(value, temp_len);

							memcpy(value->u.obj.m, context->stack_point + cur_top, sizeof(lept_obj_member) * temp_len);
							context->stack_point = cur_top;

							return LEPT_OK;
						default:
							return LEPT_INVALID_VALUE;
						}
					}
					else {
						return LEPT_INVALID_VALUE;
					}
				}
				else {
					return LEPT_INVALID_VALUE;
				}
			}
			else {
				return LEPT_INVALID_VALUE;
			}
		}
	}
	else {
		return LEPT_INVALID_VALUE;
	}
}

static int lept_parse_value(lept_value* output, json_context* context) {
	// 这里并不可以随便释放内存，否则temp中原本保存的下属内存信息可能会被覆盖，参见数组中存在字符串等而其后面还有其他元素，原有的字符串所处的指针就会被释放掉

	// 那么到底存不存在每次解析值之前将传入的lept_value信息释放的必要呢？
	// 传入的lept_value只有两种情况：1.是一个全新的结构体；2.是一个复用的结构体
	// 全新的结构体是一定不需要释放的
	// 复用的结构体理论上也不应当释放下属内存，因为上一轮无论是什么类型（跟本轮解析类型一样与否），其存放的也都是指针
	// 而压栈的操作是结构体具体值的复制，而非指针的复制，因此当使用一个既有的lept_value进行新一轮解析时
	// 无需在意上一轮中存放的指针的下属空间，因为当前轮会直接换掉这个指针

	switch (*context->json) {
	case('n'):return lept_parse_null(output, context); // 这里因为直接就是return因此不需要再break了，否则一定要加break。
	case('t'):return lept_parse_true(output, context);
	case('f'):return lept_parse_false(output, context);
	case('\"'):return lept_parse_string(output, context);
	case('['):return lept_parse_array(output, context);
	case('{'):return lept_parse_object(output, context);
	default:return lept_parse_number(output, context);
	}
}

int lept_parse(lept_value* output, const char* inJson) {
	// 当给出的是对象得时候就用.给出的是指向对象的指针的时候就用->
	json_context context;
	context.json = inJson;
	//初始化栈
	context.stack_point = NULL;
	context.cur_len = 0;
	context.top = 0;

	assert(output != NULL);
	output->type = LEPT_NULL;
	lept_parse_whitespace(&context);

	int ret = lept_parse_value(output, &context);

	free(context.stack_point);

	return ret;
}
// ======================================================================================================================

// =====================================================生成器==========================================================
static char* lept_generate_null(const lept_value* inValue, size_t* length) {
	*length = 4;
	return "null";
}

static char* lept_generate_true(const lept_value* inValue, size_t* length) {
	*length = 4;
	return "true";
}

static char* lept_generate_false(const lept_value* inValue, size_t* length) {
	*length = 5;
	return "false";
}

static char* lept_generate_number(const lept_value* inValue, size_t* length) {
	char* buffer = (char*)malloc(32);
	//char buffer[32];  // 申请内存不一定非要malloc，可以直接以数组的形式申请，但是这个方式在函数外会被释放，malloc不会
	*length = sprintf(buffer, "%.10lf", inValue->u.num);
	return buffer;
}

static char* lept_generate_string(const lept_value* inValue, size_t* length) {
	*length = inValue->u.str.len + 2;

	char* output = (char*)malloc(inValue->u.str.len + 3);
	memcpy(output + 1, inValue->u.str.s, inValue->u.str.len);
	output[0] = '\"';
	output[inValue->u.str.len + 1] = '\"';
	output[inValue->u.str.len + 2] = '\0';

	return output;
}

static char* lept_generate_array(json_context* context, const lept_value* inValue, size_t* length) {
	size_t cur_top = context->top;  // 保存当前的栈顶

	size_t total_size = 0;

	int cur_index;
	char* temp_char;
	size_t temp_size;
	char* temp_full_char;

	for (cur_index = 0; cur_index < inValue->u.arr.elements_count; cur_index++) {
		lept_value* cur_element = inValue->u.arr.a + cur_index;
		temp_char = lept_generate(cur_element, &temp_size);

		memcpy((char*)get_point(context, temp_size), temp_char, temp_size);
		total_size += temp_size;

		*(char*)get_point(context, sizeof(char)) = ',';
		total_size += 1;
	}

	char* real_output = (char*)malloc(total_size + 2);
	memcpy(real_output + 1, context->stack_point + cur_top, total_size);
	real_output[0] = '[';
	real_output[total_size] = ']';
	real_output[total_size + 1] = '\0';

	*length = total_size + 1;

	context->top = cur_top;

	return real_output;
}

static char* lept_generate_object(json_context* context, const lept_value* inValue, size_t* length) {
	size_t cur_top = context->top;  // 保存当前的栈顶

	int cur_index;
	lept_obj_member* temp_member;
	char* temp_value_char;
	size_t temp_length;

	size_t total_size = 0;

	for (cur_index = 0; cur_index < inValue->u.obj.members_count; cur_index++) {
		temp_member = inValue->u.obj.m + cur_index;

		// 首先将key压栈
		memcpy((char*)get_point(context, temp_member->key_len + 2) + 1, temp_member->member_key, temp_member->key_len);
		*(context->stack_point + cur_top + total_size) = '\"';
		*(context->stack_point + cur_top + total_size + (temp_member->key_len) + 1) = '\"';
		total_size += (temp_member->key_len + 2);

		// 压栈一个":"
		*(char*)get_point(context, sizeof(char)) = ':';
		total_size += 1;

		// 解析成员值并将其压栈
		temp_value_char = lept_generate(&temp_member->member_value, &temp_length);
		memcpy((char*)get_point(context, temp_length), temp_value_char, temp_length);
		total_size += temp_length;

		// 在每个元素末尾压入一个","
		*(char*)get_point(context, sizeof(char)) = ',';
		total_size += 1;
	}

	// 元素内部值生成完后，再分配空间保存栈内信息，复位栈顶指针，并将内部信息两侧加上{}和\0
	char* real_output = malloc(total_size + 2);
	memcpy(real_output + 1, context->stack_point + cur_top, total_size);
	real_output[0] = '{';
	real_output[total_size] = '}';
	real_output[total_size + 1] = '\0';

	*length = total_size + 1;

	context->top = cur_top;

	return real_output;
}

// 函数如果存在指针式返回值，那么其下属的空间有三种情况
//1.在函数体内以char a[32]方式申请——这种方式申请的内存是在栈上的，空间在函数体外立刻就会释放，数据会损坏
//2.在函数体内以malloc(32)方式申请——这种方式申请的内存是在文件映射区或堆上的，空间不会在函数体外释放，数据也不会释放
//3.在函数结尾直接return "XXX"——这种方式"XXX"所在的内存也不会被释放，但存储区域暂未知。
char* lept_generate(const lept_value* inValue, size_t* length) {
	//初始化栈-此处的栈仅用来处理array和object
	json_context context;
	context.json = NULL;
	context.stack_point = NULL;
	context.cur_len = 0;
	context.top = 0;

	switch (inValue->type) {
	case(LEPT_NULL):return lept_generate_null(inValue, length);
	case(LEPT_TRUE):return lept_generate_true(inValue, length);
	case(LEPT_FALSE):return lept_generate_false(inValue, length);
	case(LEPT_NUMBER):return lept_generate_number(inValue, length);
	case(LEPT_STRING):return lept_generate_string(inValue, length);
	case(LEPT_ARRAY):return lept_generate_array(&context, inValue, length);
	case(LEPT_OBJECT):return lept_generate_object(&context, inValue, length);
	}
}
// =====================================================================================================================


// ===========================================用户对某个value的值操作接口===============================================
// ==============================通用======================================
lept_type lept_get_type(lept_value* value) {
	return value->type;
}
// ========================================================================

// ================================空======================================
int lept_init_null(lept_value* value) {
	value->type = LEPT_NULL;
	return LEPT_OK;
}
// ========================================================================

// ==============================开关量====================================
int lept_init_boolen(lept_value* value) {
	value->type = LEPT_TRUE;
	return LEPT_OK;
}

int lept_get_boolen(const lept_value* value) {
	// 所有get类函数都要求所传入的value类型一定要正确，否则就直接报错（不然getType函数做来干嘛用的）
	assert(value->type == LEPT_FALSE || value->type == LEPT_TRUE);
	if (value->type == LEPT_TRUE) {
		return 1;
	}
	else {
		return 0;
	}
}

void lept_set_boolen(lept_value* value, int b) {
	assert(value->type == LEPT_FALSE || value->type == LEPT_TRUE);
	if (b) {
		value->type = LEPT_TRUE;
	}
	else {
		value->type = LEPT_FALSE;
	}
}
// ========================================================================

// ==============================数值======================================
int lept_init_number(lept_value* value) {
	value->type = LEPT_NUMBER;
	value->u.num = 0;
	return LEPT_OK;
}

double lept_get_number(const lept_value* value) {
	assert(value->type == LEPT_NUMBER);
	return value->u.num;
}

void lept_set_number(lept_value* value, double num) {
	assert(value->type == LEPT_NUMBER);
	value->u.num = num;
}
// ========================================================================

// ==============================字符串====================================
int lept_init_string(lept_value* value) {
	value->type = LEPT_STRING;
	value->u.str.s = NULL;
	value->u.str.len = 0;
	return LEPT_OK;
}

char* lept_get_string(const lept_value* value) {
	assert(value->type == LEPT_STRING);
	return value->u.str.s;
}

size_t lept_get_string_length(const lept_value* value) {
	assert(value->type == LEPT_STRING);
	return value->u.str.len;
}

void lept_set_string(lept_value* value, char* str, size_t str_len) {
	assert(value->type == LEPT_STRING);

	//设置字符串函数的整体流程是：首先确保value的类型正确，其次将value中原有的指针释放，重新申请特定长度的内存，
	//再将给出指针的给定长度复制进原指针中。
	//另外默认这个strlen是给出的不带\0的长度。
	lept_free(value);
	value->u.str.s = (char*)malloc(str_len + 1);
	memcpy(value->u.str.s, str, str_len);
	value->u.str.s[str_len] = '\0';
	value->u.str.len = str_len;
}
// ========================================================================

// ==============================数组======================================
int lept_init_array(lept_value* value) {
	value->type = LEPT_ARRAY;
	value->u.arr.a = NULL;
	value->u.arr.elements_count = 0;
	value->u.arr.capability = 0;
	return LEPT_OK;
}

// 该函数用于将普通数组进化为动态数组，数组的elements_count是实际长度，而capability是分配的可用动态长度（大于等于elements_count）
// 该函数在解析器中仅改变弹栈后的信息保存方法（将空间申请交给本函数），其余的更多调用是在append功能中出现的。
// 无需返回头指针，因为待修改的指针一定会被重新赋值一遍（哪怕是原样赋值）（注意为了避免把null赋值上去，要做一个临时指针检查结果后再赋值）
int lept_set_array_capability(lept_value* value, size_t cap) {
	// 如果是个空的数组，也可以直接被ralloc处理，因此不用单独列分支
	// 如果需要的cap比原有的大，才进行处重新分配处理，否则返回分配失败或直接返回原值（输入cap相等时）
	if (cap > value->u.arr.capability) {
		lept_value* temp_array = (lept_value*)realloc(value->u.arr.a, cap * sizeof(lept_value));
		if (temp_array != NULL) {
			value->u.arr.a = temp_array;
			value->u.arr.capability = cap;
			return LEPT_OK;
		}
		return LEPT_OUT_OF_MEMORY;
	}
	else if (cap > value->u.arr.capability) {
		return LEPT_OK;
	}
	else {
		return LEPT_SMALL_THAN_CARRENT_CAP;
	}
}

lept_value* lept_get_array_value(lept_value* value, size_t index) {
	assert(value->type == LEPT_ARRAY && index < value->u.arr.elements_count);
	return &value->u.arr.a[index];
}

size_t lept_get_array_elements_count(const lept_value* value) {
	assert(value->type == LEPT_ARRAY);
	return value->u.arr.elements_count;
}

size_t lept_get_array_capability(const lept_value* value) {
	return value->u.arr.capability;
}

size_t lept_set_array_value(lept_value* value, lept_value* target, size_t index) {
	if (index < value->u.arr.elements_count) {
		lept_swap(value->u.arr.a + index, target->u.arr.a + index);
		return LEPT_OK;
	}
	return LEPT_ELEMENT_NOT_EXIST;

	//assert(value->type == LEPT_ARRAY && index < value->u.arr.elements_count);
	//// 目标是将value数组中的第index个地址改成target的地址。而[]操作取出来的直接就是这个结构体本身（虽然本质上仍是指针）
	//// 因此将这个“值”改成target的地址即可达成目标。————这个做法就会导致指针共用（同一个地址被来回引用）而无法正确释放《改成swap》
	//value->u.arr.a[index] = *target;
}

// 基于move方法，若成功添加，被添加的指针将被置空
int lept_append_array_elelment(lept_value* value, lept_value* target) {
	// 如果capability充足，则直接move
	if (value->u.arr.capability > value->u.arr.elements_count) {
		lept_move(value->u.arr.a + value->u.arr.elements_count, target);
		value->u.arr.elements_count++;
		return LEPT_OK;
	}
	// 否则，需要使用set-capability函数分配内存，
	else {
		int ret = lept_set_array_capability(value, value->u.arr.capability + ((value->u.arr.capability) >> 1) + 1);  // 同样1.5倍扩增原有空间
		if (ret == LEPT_OK) {
			lept_append_array_elelment(value, target);  // 这里直接递归append得了
		}
		return ret;
	}
}

int lept_shrink_array(lept_value* value) {
	if (value->u.arr.elements_count <= value->u.arr.capability) {
		lept_value* temp_array = realloc(value->u.arr.a, value->u.arr.elements_count * sizeof(lept_value));
		if (temp_array != NULL) {
			value->u.arr.a = temp_array;
			value->u.arr.capability = temp_array;
			return LEPT_OK;
		}
		return LEPT_OUT_OF_MEMORY;
	}
	return LEPT_INVALID_VALUE;
}

lept_value* lept_pop_array_elelment(lept_value* value) {
	if (value->u.arr.elements_count != 0) {
		lept_value* output = (lept_value*)malloc(sizeof(lept_value));
		lept_move(output, value->u.arr.a + value->u.arr.elements_count - 1);
		value->u.arr.elements_count--;
		return output;
	}
	return NULL;
}

int lept_insert_array(lept_value* value, lept_value* target, size_t index) {
	// 首先复制一份数组的末尾值重复append进数组，而后将倒数第一个末尾值冒泡到index位置《记得处理数组还是空的时候》
	lept_value* temp_end_element = (lept_value*)malloc(sizeof(lept_value));
	lept_copy(temp_end_element, value->u.arr.a + value->u.arr.elements_count - 1);

	int ret = lept_append_array_elelment(value, temp_end_element);
	lept_value* v1, * v2;
	if (ret == LEPT_OK) {
		// 共需要swap原尾index-目标index次
		int cur_index;
		for (cur_index = value->u.arr.elements_count - 2; cur_index > index; cur_index) {
			v1 = value->u.arr.a + cur_index;
			v2 = value->u.arr.a + cur_index - 1;
			lept_swap(v1, v2);
		}
		// 将目标value move进目标位置
		lept_move(value->u.arr.a + index, target);
	}
	return ret;
}

size_t lept_remove_array_element(lept_value* value, size_t index) {
	if (index <= value->u.arr.elements_count - 1) {
		// 将待弹出的目标swap到末尾
		int cur_index;
		for (cur_index = index; cur_index < value->u.arr.elements_count - 1; cur_index++) {
			lept_swap(value->u.arr.a + index, value->u.arr.a + index + 1);
		}
		lept_pop_array_elelment(value);
		return LEPT_OK;
	}
	return LEPT_ELEMENT_NOT_EXIST;
}
// ========================================================================

// ==============================对象======================================
int lept_init_obj(lept_value* value) {
	value->type = LEPT_OBJECT;
	value->u.obj.m = NULL;
	value->u.obj.members_count = 0;
	value->u.obj.capability = 0;
	return LEPT_OK;
}

int lept_set_obj_capability(lept_value* value, size_t cap) {
	if (cap > value->u.obj.capability) {
		lept_value* temp_array = (lept_value*)realloc(value->u.obj.m, cap * sizeof(lept_value));
		if (temp_array != NULL) {
			value->u.obj.m = temp_array;
			value->u.obj.capability = cap;
			return LEPT_OK;
		}
		return LEPT_OUT_OF_MEMORY;
	}
	else if (cap > value->u.obj.capability) {
		return LEPT_OK;
	}
	else {
		return LEPT_SMALL_THAN_CARRENT_CAP;
	}
}

size_t lept_get_obj_members_count(lept_value* value) {
	return value->u.obj.members_count;
}

const char* lept_get_obj_key(lept_value* value, size_t index) {
	return (value->u.obj.m + index)->member_key;
}

size_t lept_get_obj_key_len(lept_value* value, size_t index) {
	return (value->u.obj.m + index)->key_len;
}

lept_value* lept_get_obj_value(lept_value* value, size_t index) {
	if (index < value->u.obj.members_count) {
		return &(value->u.obj.m + index)->member_value;
	}
	return NULL;
}

size_t lept_find_obj_key(const lept_value* value, const char* target_key, size_t tklen) {
	int temp_index;
	lept_obj_member* temp_member;
	for (temp_index = 0; temp_index < value->u.obj.members_count; temp_index++) {
		temp_member = value->u.obj.m + temp_index;
		if (temp_member->key_len == tklen && memcmp(temp_member->member_key, target_key, tklen)) {
			return temp_index;
		}
	}
	return LEPT_ELEMENT_NOT_EXIST;
}

lept_value* lept_find_obj_member(const lept_value* value, const char* target_key, size_t tklen) {
	size_t target_index = lept_find_obj_key(value, target_key, tklen);
	if (target_index != LEPT_ELEMENT_NOT_EXIST) {
		return lept_get_obj_value(value, target_index);
	}
	return NULL;
}

int lept_is_equal(const lept_value* src_value, const lept_value* trg_value) {
	if (src_value->type == trg_value->type) {
		switch (src_value->type) {
		case(LEPT_NULL || LEPT_TRUE || LEPT_FALSE): {
			return LEPT_OK;
		}
		case(LEPT_NUMBER): {
			if (src_value->u.num == trg_value->u.num) {
				return LEPT_OK;
			}
			return 0;
		}
		case(LEPT_STRING): {
			if (src_value->u.str.len == trg_value->u.str.len && memcmp(src_value->u.str.s, trg_value->u.str.s, src_value->u.str.len)) {
				return LEPT_OK;
			}
			return 0;
		}
		case(LEPT_ARRAY): {  // 数组需要满足各个下标也须在同位
			if (src_value->u.arr.elements_count == trg_value->u.arr.elements_count) {
				int output = 1;
				int temp_index;

				for (temp_index = 0; temp_index < src_value->u.arr.elements_count; temp_index++) {
					output = output & lept_is_equal(src_value->u.arr.a + temp_index, trg_value->u.arr.a + temp_index);
				}
				return output;
			}
			return 0;
		}
		case(LEPT_OBJECT): {
			if (src_value->u.obj.members_count == trg_value->u.obj.members_count) {
				int output = 1;
				lept_value* temp_value;
				int temp_index;
				for (temp_index = 0; temp_index < src_value->u.obj.members_count; temp_index++) {
					temp_value = lept_find_obj_member(trg_value, (src_value->u.obj.m + temp_index)->member_key, (src_value->u.obj.m + temp_index)->key_len);
					if (temp_value == NULL) {
						return 0;
					}
					output = output & lept_is_equal(&(src_value->u.obj.m + temp_index)->member_value, temp_value);
				}
				return output;
			}
		}
		}
	}
	return 0;
}

size_t lept_set_obj_value(lept_value* trg_obj, lept_obj_member* src_member) {
	lept_obj_member* trg_member = lept_find_obj_member(trg_obj, src_member->member_key, src_member->key_len);
	if (trg_member != NULL) {
		lept_swap(&trg_member->member_value, &src_member->member_value);
		return LEPT_OK;
	}
	return LEPT_ELEMENT_NOT_EXIST;
}

int lept_append_obj_member(lept_value* value, const char* target_key, size_t tklen, lept_value* src_member_value) {
	// obj属于无序表，因此并不用在意这个member到底被安在那里了
	size_t cur_len = value->u.obj.members_count;
	size_t cur_cap = value->u.obj.capability;

	// 容量够的时候则直接append进末尾
	if (cur_len < cur_cap) {
		lept_obj_member* temp_member = value->u.obj.m + cur_len;
		temp_member->key_len = tklen;
		memcpy(temp_member->member_key, target_key, tklen);
		lept_move(&temp_member->member_value, src_member_value);
		value->u.obj.members_count++;
		return LEPT_OK;
	}
	else {
		int ret = lept_set_obj_capability(value, cur_cap + (cur_cap >> 1) + 1);
		if (ret == LEPT_OK) {
			ret = lept_append_obj_member(value, target_key, tklen, src_member_value);
			return ret;
		}
		return ret;
	}
}

int lept_shrink_obj(lept_value* value) {
	if (value->u.obj.capability > value->u.obj.members_count) {
		lept_value* temp_obj = (lept_value*)realloc(value->u.obj.m, value->u.obj.members_count * sizeof(lept_obj_member));
		if (temp_obj != NULL) {
			value->u.obj.m = temp_obj;
			value->u.obj.capability = value->u.obj.members_count;
			return LEPT_OK;
		}
		return LEPT_OUT_OF_MEMORY;
	}
	return LEPT_INVALID_VALUE;
}

size_t lept_remove_obj_member(lept_value* value, const char* target_key, size_t tklen) {
	size_t del_index = lept_find_obj_key(value, target_key, tklen);
	if (del_index == LEPT_OK) {  // 查找到了要remove的项再进行后续处理，否则直接返回错误码
		lept_obj_member* temp_mp = value->u.obj.m;

		int cur_index;
		char* temp_char_p;
		size_t temp_klen;
		for (cur_index = (int)del_index; cur_index < value->u.obj.members_count - 1; cur_index++) {

			// 交换key名
			temp_char_p = (temp_mp + cur_index)->member_key;
			(temp_mp + cur_index)->member_key = (temp_mp + cur_index + 1)->member_key;
			(temp_mp + cur_index + 1)->member_key = temp_char_p;

			// 交换key长
			temp_klen = (temp_mp + cur_index)->key_len;
			(temp_mp + cur_index)->key_len = (temp_mp + cur_index + 1)->key_len;
			(temp_mp + cur_index + 1)->key_len = temp_klen;

			// 交换value值
			lept_swap(&(temp_mp + cur_index)->member_value, &(temp_mp + cur_index + 1)->member_value);
		}

		// 将元素总数-1,释放出被remove元素的使用权
		value->u.obj.members_count--;
		return LEPT_OK;
	}
	return del_index;
}
// ========================================================================
// ======================================================================================================================