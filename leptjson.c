#include "leptjson.h"
#include <assert.h>
#include <stdlib.h>
#include<malloc.h>
#include<string.h>

// 先定义一个context对象，用来后面来回传，基本上所有函数的传参都由这个context负责。
// 将栈定义在这个结构体里面是为了栈复用，而所有地方压入栈的方式都是一样的，都是操作栈顶指针，唯一区别的地方就是
// 需要在刚拿到栈的时候记录一下栈顶指针的位置，回头退出的时候把指针退到这个位置上（栈顶指针复位）
typedef struct {
	char* json;
	char* stack_point;  // 定义栈的首地址
	size_t top;  // 定义栈顶指针的位置（栈顶指针指向的就是下一个要赋值操作的地址）
	size_t cur_len;  // 定义指针所指向的内存总长
}json_context;

// ==================================================栈与指针操作=====================================================

// 清空一个现有的value结构体下的内存（如果不具有下属内存就无需清理）
void lept_free(lept_value* value) {

	// 如果类型是三个不定长类，就释放内存，且重置相关变量。
	switch (value->type) {
	case(LEPT_STRING):
		value->u.str.len = 0;
		free(value->u.str.s);  // 注意free不能free一个野指针，至少需要是NULL才能被free
		return;
	case(LEPT_ARRAY):
		value->u.arr.elements_count = 0;
		free(value->u.arr.a);  // 直接free这个指针能行吗？按理说这个指针下是存在总长的，总长里面就是全部的value结构体长度（而非只有指针的长度）
		return;
	case(LEPT_OBJECT):
		value->u.obj.members_count = 0;
		free(value->u.obj.m);
		return;
	default:return;
	}
}

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
			context->stack_point = (char*)realloc(context->stack_point, context->cur_len);
			size_t move = context->top;
			context->top += ex_size;
			return context->stack_point + move;
		}
	}
}
// ===================================================================================================================

// ===================================================解析器部分======================================================
static void lept_prase_whitespace(json_context* context) {
	char* real_json_word = context->json;
	// 注意所有的转义字符，其本质上在计算机内都是一个字符！
	while (*real_json_word == ' ' || *real_json_word == '\t' || *real_json_word == '\r' || *real_json_word == '\n') {
		real_json_word++;
	}
	context->json = real_json_word;
}

static int lept_prase_null(lept_value* output, json_context* context) {
	// 注意从指针里面直接偏移(偏移运算符本身带取值功能)，就自动会取值了，不需要再加*
	if (context->json[0] != 'n' || context->json[1] != 'u' || context->json[2] != 'l' || context->json[3] != 'l') {
		return LEPT_INVALID_VALUE;
	}
	output->type = LEPT_NULL;
	context->json += 4;
	return LEPT_OK;
}

static int lept_prase_true(lept_value* output, json_context* context) {
	if (context->json[0] != 't' || context->json[1] != 'r' || context->json[2] != 'u' || context->json[3] != 'e') {
		return LEPT_INVALID_VALUE;
	}
	output->type = LEPT_TRUE;
	context->json += 4;
	return LEPT_OK;
}

static int lept_prase_false(lept_value* output, json_context* context) {
	if (context->json[0] != 'f' || context->json[1] != 'a' || context->json[2] != 'l' || context->json[3] != 's' || context->json[4] != 'e') {
		return LEPT_INVALID_VALUE;
	}
	output->type = LEPT_FALSE;
	context->json += 5;
	return LEPT_OK;
}

static int lept_prase_number(lept_value* output, json_context* context) {
	char* prased_char;
	output->u.num = strtod(context->json, &prased_char);
	if (prased_char == context->json) {
		return LEPT_INVALID_VALUE;
	}
	context->json = prased_char;
	output->type = LEPT_NUMBER;
	return LEPT_OK;
}

static int lept_prase_string(lept_value* value, json_context* context) {
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
			
			value->type = LEPT_STRING;
			value->u.str.len = 0;
			value->u.str.s = NULL;  // 为了不让这个s指针是野指针（野指针不能直接free），需要在实际使用前先让这个指针指向某些区域,NULL也行，malloc(1)也行但最好是NULL。
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

static int lept_prase_array(lept_value* value, json_context* context) {
	// 保存一份当前的栈顶
	size_t cur_top = context->top;

	// 偏移指针拿掉最开头的[
	context->json++;
	lept_prase_whitespace(context);  // 去一下可能存在在符号之后的空格。

	lept_value temp_value;  // 创建一个临时的value，这个value就是将要保存一会嵌套prase并且最终压进栈的value。
	size_t temp_len = 0;

	// 死循环，只要没到下一个左分号，就说明当前array还没有结束，循环还需要继续
	while (1) {
		int ret = lept_prase_value(&temp_value, context);  // 把当前的这个元素解析出来存放在临时的temp_value中
		// 如果成功解析了，则进入保存阶段
		if (ret == LEPT_OK) {
			*(lept_value*)get_point(context, sizeof(temp_value)) = temp_value;  // 把解析出来的临时value压入栈中（注意这个操作是将“指针指向的那段区域的指针”的“值”赋值为了一个既有结构体。本质上是“A结构体”=“B结构体”，即使用一个结构体来赋值另一个结构体，这个并不是所有数据类型都支持这么操作，至少字符串不行）
			temp_len++;  // 只要成功解析了一个元素，就将当前数组的临时长度+1
			
			switch (*context->json) {
			case(','): 
				context->json++;
				lept_prase_whitespace(context);  // 把可能存在的空白解析掉

				continue;  // 如果是逗号，就把这个逗号跳过，直接进行下一轮（下一轮里面会自己解析掉空格）
			case(']'): 
				context->json++;
				lept_prase_whitespace(context);  // 处理当前json的结尾空格与】符
				value->type = LEPT_ARRAY;
				value->u.arr.elements_count = temp_len;
				value->u.arr.a = malloc(sizeof(temp_value) * temp_len);
				memcpy(value->u.arr.a, context->stack_point + cur_top, sizeof(temp_value) * temp_len);  // 将栈中的东西复制进结果中
				context->top = cur_top;  // 复位栈顶指针
				return LEPT_OK;
			}
		}
		else {
			return LEPT_INVALID_VALUE;
		}
	}

	}

static int lept_prase_object(lept_value* value, json_context* context) {
	// 该解析函数同样需要公用栈，因此保存栈顶指针位置
	size_t cur_top = context->cur_len;

	// 定义并初始化一个临时的对象成员
	lept_obj_member temp_member;
	temp_member.member_key = NULL;
	temp_member.key_len = 0;
	temp_member.member_value = NULL;

	// 定义一个临时value用于保存key的字符串信息。
	lept_value temp_key;

	// 定义返回值保存变量
	int ret;

	size_t temp_len = 0;

	while (1) {
		// 解析key
		ret = lept_prase_string(&temp_key, context);
		if (ret == LEPT_OK) {
			lept_prase_whitespace(context);
			if (*context->json == ':') {
				context->json++;  // 跳过这个冒号
				lept_prase_whitespace(context);  // 再次解析掉可能存在的空格
				ret = lept_prase_value(&temp_member.member_value, context);
				if (ret == LEPT_OK) {
					// 如果值也成功解析，则该成员解析完毕，开始保存环节（这里必须是值复制，因为temp_key是复用的，不能直接用地址进行赋值）
					free(temp_member.member_key);  // 先将（可能）原有的成员名释放掉
					temp_member.member_key = (char*)malloc(temp_key.u.str.len);  // 重新分配内存
					memcpy(temp_member.member_key, temp_key.u.str.s, temp_key.u.str.len);
					temp_member.key_len = temp_key.u.str.len;

					// 所有信息都已经保存进temp_member中，可以释放temp_key中的相关信息
					// （lept_prase_string函数中会直接将这个char指针置为NULL，而char原本指向的内存是不会释放的，就会内存泄漏）
					free(temp_key.u.str.s);

					// 此时栈是干净的（所有要弹出的东西理论上已经都弹出了），因此开始将这个临时member压栈
					*(lept_obj_member*)get_point(context, sizeof(lept_obj_member)) = temp_member;
					temp_len++;

					// 查看当前context中是“，”还是“}”，以此判断是否要继续循环
					switch (*context->json) {
					case(','):;
					case(']'):;
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

	return LEPT_OK;
}

static int lept_prase_value(lept_value* output, json_context* context) {
	// 先将可能存在的下属内存释放，并将类型置为空
	if (output->type != LEPT_NULL) {
		lept_free(output);
		output->type = LEPT_NULL;
	}

	switch (*context->json) {
	case('n'):return lept_prase_null(output, context); // 这里因为直接就是return因此不需要再break了，否则一定要加break。
	case('t'):return lept_prase_true(output, context);
	case('f'):return lept_prase_false(output, context);
	case('\"'):return lept_prase_string(output, context);
	case('['):return lept_prase_array(output, context);
	case('{'):return lept_prase_object(output, context);
	default:return lept_prase_number(output, context);
}
}

int lept_prase(lept_value* output, const char* inJson) {
	// 当给出的是对象得时候就用.给出的是指向对象的指针的时候就用->
	json_context context;
	context.json = inJson;
	//初始化栈
	context.stack_point = NULL;
	context.cur_len = 0;
	context.top = 0;

	assert(output != NULL);
	output->type = LEPT_NULL;
	lept_prase_whitespace(&context);

	int ret = lept_prase_value(output, &context);

	free(context.stack_point);

	return ret;
}
// ======================================================================================================================


lept_type lept_get_type(lept_value* value) {
	return value->type;
}




// 用户对某个value的设定接口
int lept_get_boolen(const lept_value* value) {
	// 所有get类函数都要求所传入的value类型一定要正确，否则就直接报错（不然getType函数做来干嘛用的）
	assert(value->type == LEPT_FALSE || value->type == LEPT_TRUE);
	if (value->type == LEPT_TRUE) {
		return 1;
	}
	else{
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

double lept_get_number(const lept_value* value) {
	assert(value->type == LEPT_NUMBER);
	return value->u.num;
}

void lept_set_number(lept_value* value, double num) {
	assert(value->type == LEPT_NUMBER);
	value->u.num = num;
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

lept_value lept_get_array_value(lept_value* value, size_t index) {
	assert(value->type == LEPT_ARRAY && index < value->u.arr.elements_count);
	return value->u.arr.a[index];
}

size_t lept_get_array_length(const lept_value* value) {
	assert(value->type == LEPT_ARRAY);
	return value->u.arr.elements_count;
}

void lept_set_arrray_value(lept_value* value, lept_value* target, size_t index) {
	assert(value->type == LEPT_ARRAY && index < value->u.arr.elements_count);
	// 目标是将value数组中的第index个地址改成target的地址。而[]操作取出来的直接就是这个结构体本身（虽然本质上仍是指针）
	// 因此将这个“值”改成target的地址即可达成目标。
	value->u.arr.a[index] = *target;
}