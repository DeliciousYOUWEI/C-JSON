#include "leptjson.h"
#include <assert.h>
#include <stdlib.h>
#include<malloc.h>
#include<string.h>

// �ȶ���һ��context���������������ش������������к����Ĵ��ζ������context����
// ��ջ����������ṹ��������Ϊ��ջ���ã������еط�ѹ��ջ�ķ�ʽ����һ���ģ����ǲ���ջ��ָ�룬Ψһ����ĵط�����
// ��Ҫ�ڸ��õ�ջ��ʱ���¼һ��ջ��ָ���λ�ã���ͷ�˳���ʱ���ָ���˵����λ���ϣ�ջ��ָ�븴λ��
typedef struct {
	char* json;
	char* stack_point;  // ����ջ���׵�ַ
	size_t top;  // ����ջ��ָ���λ�ã�ջ��ָ��ָ��ľ�����һ��Ҫ��ֵ�����ĵ�ַ��
	size_t cur_len;  // ����ָ����ָ����ڴ��ܳ�
}json_context;

// ջ��ָ�����
void lept_free(lept_value* value) {

	// ��������������������࣬���ͷ��ڴ棬��������ر�����
	if (value->type == LEPT_STRING) {
		value->u.str.len = 0;
		free(value->u.str.s);  // �����free�ռ����
	}
	else if (value->type == LEPT_ARRAY) {

	}
}

// ��ѽṹ�崫Ϊ���������û��typedef���Ǿ�Ҫд��struct XX*�����ʹ����typedef����ֱ����XX*�����ˡ�
// �ú�����������ջ���浱ǰ�������ڲ����ָ�뷵�ص�������void���͵�ָ�룬��������ת����Ҫת������͡�
static void* get_point(json_context* context, size_t ex_size) {

	// ������Ǹ��´�����ջ������Ҫ��ջ��ʼ�����ȣ�ͬʱ��cur_len��ֵ�������ص�ǰ��pointλ��
	if (context->cur_len == 0) {
		context->stack_point = (char*)malloc(256);
		context->cur_len = 256;
		context->top += ex_size;
		return context->stack_point;
	}

	// ��������´�����ջ���Ͳ鿴��ǰҪ�����ĵ�ַ�Ƿ���������ĳ�����
	else {
		// �������뷶Χ�ڣ��򷵻ص�ǰ��������ַ������ջ��ָ������
		if (context->top < context->cur_len) {
			size_t move = context->top;
			context->top += ex_size;
			return context->stack_point + move;
		}
		// ����������뷶Χ���ˣ������������ڴ棨��������1.5��
		else {
			context->cur_len += (context->cur_len >> 1);
			context->stack_point = (char*)realloc(context->stack_point, context->cur_len);
			size_t move = context->top;
			context->top += ex_size;
			return context->stack_point + move;
		}
	}
}


static void lept_prase_whitespace(json_context* context) {
	char* real_json_word = context->json;
	// ע�����е�ת���ַ����䱾�����ڼ�����ڶ���һ���ַ���
	while (*real_json_word == ' ' || *real_json_word == '\t' || *real_json_word == '\r' || *real_json_word == '\n') {
		real_json_word++;
	}
	context->json = real_json_word;
}

static int lept_prase_null(lept_value* output, json_context* context) {
	// ע���ָ������ֱ��ƫ��(ƫ������������ȡֵ����)�����Զ���ȡֵ�ˣ�����Ҫ�ټ�*
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
	// ���浱ǰ��ջ��ָ��λ��
	size_t cur_top = context->top;


	//ǰ��һλȥ����ǰ���˫���š�
	context->json++;

	// ��ѭ����ֻҪû����һ��\"���㻹û������ǰ�ַ�����ע�⣬�ǲ���Ҫ�б�ת���ַ��ģ���Ϊ�������ת�壬���������ʽ��"
	while (1) {
		switch (*context->json) {
		case '\"':
			//���������һ��\"�ˣ����˫���žͲ�����������ˣ�ֱ�ӿ�ʼ��ջ�е���Ϣת����û���Ŀ��ṹ���У�Ȼ���ͷ���Դ�˳�
			context->json++;  // ��������һ��˫����
			
			value->type = LEPT_STRING;
			value->u.str.len = 0;
			value->u.str.s = NULL;  // Ϊ�˲������sָ����Ұָ�루Ұָ�벻��ֱ��free������Ҫ��ʵ��ʹ��ǰ�������ָ��ָ��ĳЩ����,NULLҲ�У�malloc(1)Ҳ�е������NULL��
			lept_set_string(value, context->stack_point + cur_top, context->top - cur_top);
			context->top = cur_top;  // ָ����ͷŲ����ٷ��������Ϊ�����Ѿ�������ջ�ĳ������õĵط��ˣ�����������Ҫ��ջ���سɺ������õ�ʱ���״̬��
			return LEPT_OK;

		default:
			*(char*)get_point(context, sizeof(*context->json)) = *context->json;
			context->json++;
			continue;
		}
	}

}

static int lept_prase_array(lept_value* value, json_context* context) {
	// ����һ�ݵ�ǰ��ջ��
	size_t cur_top = context->top;

	// ƫ��ָ���õ��ͷ��[
	context->json++;
	lept_prase_whitespace(context);  // ȥһ�¿��ܴ����ڷ���֮��Ŀո�

	lept_value temp_value;  // ����һ����ʱ��value�����value���ǽ�Ҫ����һ��Ƕ��prase��������ѹ��ջ��value��
	size_t temp_len = 0;

	// ��ѭ����ֻҪû����һ����ֺţ���˵����ǰarray��û�н�����ѭ������Ҫ����
	while (1) {
		int ret = lept_prase_value(&temp_value, context);  // �ѵ�ǰ�����Ԫ�ؽ��������������ʱ��temp_value��
		// ����ɹ������ˣ�����뱣��׶�
		if (ret == LEPT_OK) {
			*(lept_value*)get_point(context, sizeof(temp_value)) = temp_value;  // �ѽ�����������ʱvalueѹ��ջ��
			temp_len++;  // ֻҪ�ɹ�������һ��Ԫ�أ��ͽ���ǰ�������ʱ����+1
			
			switch (*context->json) {
			case(','): 
				context->json++;
				lept_prase_whitespace(context);  // �ѿ��ܴ��ڵĿհ׽�����

				continue;  // ����Ƕ��ţ��Ͱ��������������ֱ�ӽ�����һ�֣���һ��������Լ��������ո�
			case(']'): 
				context->json++;
				lept_prase_whitespace(context);  // ����ǰjson�Ľ�β�ո��롿��
				value->type = LEPT_ARRAY;
				value->u.arr.elements_count = temp_len;
				value->u.arr.a = malloc(sizeof(temp_value) * temp_len);
				memcpy(value->u.arr.a, context->stack_point + cur_top, sizeof(temp_value) * temp_len);  // ��ջ�еĶ������ƽ������
				context->top = cur_top;  // ��λջ��ָ��
				return LEPT_OK;
			}
		}
		else {
			return LEPT_INVALID_VALUE;
		}
	}

	}


static int lept_prase_value(lept_value* output, json_context* context) {
	switch (*context->json) {
	case('n'):return lept_prase_null(output, context); // ������Ϊֱ�Ӿ���return��˲���Ҫ��break�ˣ�����һ��Ҫ��break��
	case('t'):return lept_prase_true(output, context);
	case('f'):return lept_prase_false(output, context);
	case('\"'):return lept_prase_string(output, context);
	case('['):return lept_prase_array(output, context);
	default:return lept_prase_number(output, context);
}
}

int lept_prase(lept_value* output, const char* inJson) {
	// ���������Ƕ����ʱ�����.��������ָ������ָ���ʱ�����->
	json_context context;
	context.json = inJson;
	//��ʼ��ջ
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



lept_type lept_get_type(lept_value* value) {
	return value->type;
}




// �û���ĳ��value���趨�ӿ�
int lept_get_boolen(const lept_value* value) {
	// ����get�ຯ����Ҫ���������value����һ��Ҫ��ȷ�������ֱ�ӱ�����ȻgetType�������������õģ�
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

	//�����ַ������������������ǣ�����ȷ��value��������ȷ����ν�value��ԭ�е�ָ���ͷţ����������ض����ȵ��ڴ棬
	//�ٽ�����ָ��ĸ������ȸ��ƽ�ԭָ���С�
	//����Ĭ�����strlen�Ǹ����Ĳ���\0�ĳ��ȡ�
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
	// Ŀ���ǽ�value�����еĵ�index����ַ�ĳ�target�ĵ�ַ����[]����ȡ������ֱ�Ӿ�������ṹ�屾����Ȼ����������ָ�룩
	// ��˽������ֵ���ĳ�target�ĵ�ַ���ɴ��Ŀ�ꡣ
	value->u.arr.a[index] = *target;
}