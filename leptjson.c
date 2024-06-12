#include "leptjson.h"
#include <assert.h>
#include <stdlib.h>
#include<malloc.h>
#include<string.h>
#include<stdio.h>

// �ȶ���һ��context���������������ش������������к����Ĵ��ζ������context����
// ��ջ����������ṹ��������Ϊ��ջ���ã������еط�ѹ��ջ�ķ�ʽ����һ���ģ����ǲ���ջ��ָ�룬Ψһ����ĵط�����
// ��Ҫ�ڸ��õ�ջ��ʱ���¼һ��ջ��ָ���λ�ã���ͷ�˳���ʱ���ָ���˵����λ���ϣ�ջ��ָ�븴λ��
typedef struct {
	char* json;
	char* stack_point;  // ����ջ���׵�ַ
	size_t top;  // ����ջ��ָ���λ�ã�ջ��ָ��ָ��ľ�����һ��Ҫ��ֵ�����ĵ�ַ��
	size_t cur_len;  // ����ָ����ָ����ڴ��ܳ�
}json_context;

// �Ҳ���������init���ֿ��ܻ���������������ڴ�й¶�ĺ��������ֱ�ӽ��ú�������Ϊ���ɱ���ȡ�ĺ궨��
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


// ====================================================ָ�����======================================================
// ���һ�����е�value�ṹ���µ��ڴ棨��������������ڴ����������
void lept_free(lept_value* value) {

	// ��������������������࣬���ͷ��ڴ棬��������ر�����
	switch (value->type) {
	case(LEPT_STRING): {
		value->u.str.len = 0;
		free(value->u.str.s);  // ע��free����freeһ��Ұָ�룬������Ҫ��NULL���ܱ�free�������ָ�벻�������һ����ַ������Ҫ��һ����������������׵�ַ�����׵�ַ������������ĵ�ַ����free�ǲ��е�
		// ���ָ�����Ǵ����ܳ��ģ��ܳ��������ȫ����value�ṹ�峤�ȣ�����ֻ��ָ��ĳ���)���������е��ַ���ͷ�������еȶ�û�б��ͷţ�
		return;
	}
	case(LEPT_ARRAY): {
		int temp_index;
		for (temp_index = 0; temp_index < value->u.arr.elements_count; temp_index++) {
			lept_free(value->u.arr.a + temp_index);
		}
		free(value->u.arr.a);  // ������ͷ��б�����ռ���ڴ棬�����ѭ�����ͷŵ��б��е�ָ��ָ�����Щ�ڴ�
		value->u.arr.elements_count = 0;
		return;
	}
	case(LEPT_OBJECT): {
		int temp_index;
		for (temp_index = 0; temp_index < value->u.obj.members_count; temp_index++) {
			free((value->u.obj.m + temp_index)->member_key);
			lept_free(&(value->u.obj.m + temp_index)->member_value);  // ������Ϊ�ṹ������ŵ��Ǳ��岻��ָ�룬��˲������ͷ�һ�鱾���ˣ��������ͷŽṹ���б��ʱ���һ���ͷŵ�
		}
		free(value->u.obj.m);
		value->u.obj.members_count = 0;
		return;
	}
	default:return;
	}
}

// ��ȸ���һ��value����һ��value�������������ڴ����Ϣ�����ƣ���ȫ�������ֵָ�룩
void lept_copy(lept_value* trg_value, const lept_value* src_value) {
	lept_free(trg_value);  // �������Ŀ��value
	trg_value->type = src_value->type;
	switch (src_value->type) {
	case(LEPT_NULL || LEPT_TRUE || LEPT_FALSE): {
		return;  // �������ڲ�ֵ��ֱ�ӷ��ؾ���
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

// move��copy��ͬ���ǽ�ָ�������Ȩ�������µ�value�£����ɵ�value�ڲ�������ָ�����ΪNULL��ע��ʵ����value���������ָ��ֻ��lept_value*��lept_obj_member*��
// ���move�ǲ���Ҫ�ݹ�ģ�ֻ��Ҫ�ƽ����ϼ�ָ�������Ȩ����
void lept_move(lept_value* trg_value, lept_value* src_value) {

	// ʵ�����ƽ�����Ȩ����ֱ����trgӵ��src������ֵ��Ȼ�����src�µ�ָ��
	lept_free(trg_value);
	trg_value->type = src_value->type;
	memcpy(trg_value, src_value, sizeof(lept_value));

	lept_init(src_value);
	return;
}

// ��������value��ֵ
void lept_swap(lept_value* trg_value, lept_value* src_value) {
	lept_value temp_value;
	temp_value = *src_value;
	*src_value = *trg_value;
	*trg_value = temp_value;
}
// ==================================================================================================================

// =====================================================ջ����=======================================================
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
			context->stack_point = (char*)realloc(context->stack_point, context->cur_len);  // ����������ڴ�й©���գ�reallocֱ��ԭָ�븳ֵ�ˡ�
			size_t move = context->top;
			context->top += ex_size;
			return context->stack_point + move;
		}
	}
}
// ===================================================================================================================

// ===================================================����������======================================================
static void lept_parse_whitespace(json_context* context) {
	char* real_json_word = context->json;
	// ע�����е�ת���ַ����䱾�����ڼ�����ڶ���һ���ַ���
	while (*real_json_word == ' ' || *real_json_word == '\t' || *real_json_word == '\r' || *real_json_word == '\n') {
		real_json_word++;
	}
	context->json = real_json_word;
}

static int lept_parse_null(lept_value* output, json_context* context) {
	// ע���ָ������ֱ��ƫ��(ƫ������������ȡֵ����)�����Զ���ȡֵ�ˣ�����Ҫ�ټ�*
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

			lept_init_string(value);
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

static int lept_parse_array(lept_value* value, json_context* context) {
	// ����һ�ݵ�ǰ��ջ��
	size_t cur_top = context->top;

	// ƫ��ָ���õ��ͷ��[
	context->json++;
	lept_parse_whitespace(context);  // ȥһ�¿��ܴ����ڷ���֮��Ŀո�

	lept_value temp_value;  // ����һ����ʱ��value�����value���ǽ�Ҫ����һ��Ƕ��parse��������ѹ��ջ��value��
	size_t temp_len = 0;

	// ��ѭ����ֻҪû����һ�������ţ���˵����ǰarray��û�н�����ѭ������Ҫ����
	while (1) {
		int ret = lept_parse_value(&temp_value, context);  // �ѵ�ǰ�����Ԫ�ؽ��������������ʱ��temp_value��
		// ����ɹ������ˣ�����뱣��׶�
		if (ret == LEPT_OK) {
			*(lept_value*)get_point(context, sizeof(temp_value)) = temp_value;  // �ѽ�����������ʱvalueѹ��ջ�У�ע����������ǽ���ָ��ָ����Ƕ������ָ�롱�ġ�ֵ����ֵΪ��һ�����нṹ�塣�������ǡ�A�ṹ�塱=��B�ṹ�塱����ʹ��һ���ṹ������ֵ��һ���ṹ�壬��������������������Ͷ�֧����ô�����������ַ������У�
			temp_len++;  // ֻҪ�ɹ�������һ��Ԫ�أ��ͽ���ǰ�������ʱ����+1

			switch (*context->json) {
			case(','):
				context->json++;
				lept_parse_whitespace(context);  // �ѿ��ܴ��ڵĿհ׽�����

				continue;  // ����Ƕ��ţ��Ͱ��������������ֱ�ӽ�����һ�֣���һ��������Լ��������ո�
			case(']'):
				context->json++;
				lept_parse_whitespace(context);  // ����ǰjson�Ľ�β�ո��롿��
				lept_init_array(value);
				int ret = lept_set_array_capability(value, temp_len);
				if (ret == LEPT_OK) {
					value->u.arr.elements_count = temp_len;
					memcpy(value->u.arr.a, context->stack_point + cur_top, sizeof(temp_value) * temp_len);  // ��ջ�еĶ������ƽ������
					context->top = cur_top;  // ��λջ��ָ��
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
	// �ý�������ͬ����Ҫ����ջ����˱���ջ��ָ��λ��
	size_t cur_top = context->cur_len;

	// ���岢��ʼ��һ����ʱ�Ķ����Ա
	lept_obj_member temp_member;
	temp_member.member_key = NULL;
	temp_member.key_len = 0;

	// ����һ����ʱvalue���ڱ���key���ַ�����Ϣ��
	lept_value temp_key;

	// ���巵��ֵ�������
	int ret;

	size_t temp_len = 0;

	// ������ʼ��{�����������ܴ��ڵĿո񣬲���һ���ж��Ƿ���һ���ַ���\"������������˳�
	context->json++;
	lept_parse_whitespace(context);
	if (*context->json == '\"') {
		while (1) {
			// ����key
			ret = lept_parse_string(&temp_key, context);
			if (ret == LEPT_OK) {
				lept_parse_whitespace(context);
				if (*context->json == ':') {
					context->json++;  // �������ð��
					lept_parse_whitespace(context);  // �ٴν��������ܴ��ڵĿո�
					ret = lept_parse_value(&temp_member.member_value, context);
					if (ret == LEPT_OK) {
						// ���ֵҲ�����ɹ�����ó�Ա������ϣ���ʼ���滷��
						// temp_key��ָ��ֱ�Ӹ�ֵ��temp_member���ɣ���Ϊparse-string�Ĺ����лὫָ������ΪNULL����������ԭ�еĵ�ַ��������free�����free�Ǹ�set�����õģ�
						temp_member.member_key = temp_key.u.str.s;  // ��key��ֵ
						temp_member.key_len = temp_key.u.str.len;  // ���ַ������ȸ�ֵ

						*(lept_obj_member*)get_point(context, sizeof(lept_obj_member)) = temp_member;  // �����memberѹջ

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
	// ���ﲢ����������ͷ��ڴ棬����temp��ԭ������������ڴ���Ϣ���ܻᱻ���ǣ��μ������д����ַ����ȶ�����滹������Ԫ�أ�ԭ�е��ַ���������ָ��ͻᱻ�ͷŵ�

	// ��ô���״治����ÿ�ν���ֵ֮ǰ�������lept_value��Ϣ�ͷŵı�Ҫ�أ�
	// �����lept_valueֻ�����������1.��һ��ȫ�µĽṹ�壻2.��һ�����õĽṹ��
	// ȫ�µĽṹ����һ������Ҫ�ͷŵ�
	// ���õĽṹ��������Ҳ��Ӧ���ͷ������ڴ棬��Ϊ��һ��������ʲô���ͣ������ֽ�������һ����񣩣����ŵ�Ҳ����ָ��
	// ��ѹջ�Ĳ����ǽṹ�����ֵ�ĸ��ƣ�����ָ��ĸ��ƣ���˵�ʹ��һ�����е�lept_value������һ�ֽ���ʱ
	// ����������һ���д�ŵ�ָ��������ռ䣬��Ϊ��ǰ�ֻ�ֱ�ӻ������ָ��

	switch (*context->json) {
	case('n'):return lept_parse_null(output, context); // ������Ϊֱ�Ӿ���return��˲���Ҫ��break�ˣ�����һ��Ҫ��break��
	case('t'):return lept_parse_true(output, context);
	case('f'):return lept_parse_false(output, context);
	case('\"'):return lept_parse_string(output, context);
	case('['):return lept_parse_array(output, context);
	case('{'):return lept_parse_object(output, context);
	default:return lept_parse_number(output, context);
	}
}

int lept_parse(lept_value* output, const char* inJson) {
	// ���������Ƕ����ʱ�����.��������ָ������ָ���ʱ�����->
	json_context context;
	context.json = inJson;
	//��ʼ��ջ
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

// =====================================================������==========================================================
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
	//char buffer[32];  // �����ڴ治һ����Ҫmalloc������ֱ�����������ʽ���룬���������ʽ�ں�����ᱻ�ͷţ�malloc����
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
	size_t cur_top = context->top;  // ���浱ǰ��ջ��

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
	size_t cur_top = context->top;  // ���浱ǰ��ջ��

	int cur_index;
	lept_obj_member* temp_member;
	char* temp_value_char;
	size_t temp_length;

	size_t total_size = 0;

	for (cur_index = 0; cur_index < inValue->u.obj.members_count; cur_index++) {
		temp_member = inValue->u.obj.m + cur_index;

		// ���Ƚ�keyѹջ
		memcpy((char*)get_point(context, temp_member->key_len + 2) + 1, temp_member->member_key, temp_member->key_len);
		*(context->stack_point + cur_top + total_size) = '\"';
		*(context->stack_point + cur_top + total_size + (temp_member->key_len) + 1) = '\"';
		total_size += (temp_member->key_len + 2);

		// ѹջһ��":"
		*(char*)get_point(context, sizeof(char)) = ':';
		total_size += 1;

		// ������Աֵ������ѹջ
		temp_value_char = lept_generate(&temp_member->member_value, &temp_length);
		memcpy((char*)get_point(context, temp_length), temp_value_char, temp_length);
		total_size += temp_length;

		// ��ÿ��Ԫ��ĩβѹ��һ��","
		*(char*)get_point(context, sizeof(char)) = ',';
		total_size += 1;
	}

	// Ԫ���ڲ�ֵ��������ٷ���ռ䱣��ջ����Ϣ����λջ��ָ�룬�����ڲ���Ϣ�������{}��\0
	char* real_output = malloc(total_size + 2);
	memcpy(real_output + 1, context->stack_point + cur_top, total_size);
	real_output[0] = '{';
	real_output[total_size] = '}';
	real_output[total_size + 1] = '\0';

	*length = total_size + 1;

	context->top = cur_top;

	return real_output;
}

// �����������ָ��ʽ����ֵ����ô�������Ŀռ����������
//1.�ں���������char a[32]��ʽ���롪�����ַ�ʽ������ڴ�����ջ�ϵģ��ռ��ں����������̾ͻ��ͷţ����ݻ���
//2.�ں���������malloc(32)��ʽ���롪�����ַ�ʽ������ڴ������ļ�ӳ��������ϵģ��ռ䲻���ں��������ͷţ�����Ҳ�����ͷ�
//3.�ں�����βֱ��return "XXX"�������ַ�ʽ"XXX"���ڵ��ڴ�Ҳ���ᱻ�ͷţ����洢������δ֪��
char* lept_generate(const lept_value* inValue, size_t* length) {
	//��ʼ��ջ-�˴���ջ����������array��object
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


// ===========================================�û���ĳ��value��ֵ�����ӿ�===============================================
// ==============================ͨ��======================================
lept_type lept_get_type(lept_value* value) {
	return value->type;
}
// ========================================================================

// ================================��======================================
int lept_init_null(lept_value* value) {
	value->type = LEPT_NULL;
	return LEPT_OK;
}
// ========================================================================

// ==============================������====================================
int lept_init_boolen(lept_value* value) {
	value->type = LEPT_TRUE;
	return LEPT_OK;
}

int lept_get_boolen(const lept_value* value) {
	// ����get�ຯ����Ҫ���������value����һ��Ҫ��ȷ�������ֱ�ӱ�����ȻgetType�������������õģ�
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

// ==============================��ֵ======================================
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

// ==============================�ַ���====================================
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

	//�����ַ������������������ǣ�����ȷ��value��������ȷ����ν�value��ԭ�е�ָ���ͷţ����������ض����ȵ��ڴ棬
	//�ٽ�����ָ��ĸ������ȸ��ƽ�ԭָ���С�
	//����Ĭ�����strlen�Ǹ����Ĳ���\0�ĳ��ȡ�
	lept_free(value);
	value->u.str.s = (char*)malloc(str_len + 1);
	memcpy(value->u.str.s, str, str_len);
	value->u.str.s[str_len] = '\0';
	value->u.str.len = str_len;
}
// ========================================================================

// ==============================����======================================
int lept_init_array(lept_value* value) {
	value->type = LEPT_ARRAY;
	value->u.arr.a = NULL;
	value->u.arr.elements_count = 0;
	value->u.arr.capability = 0;
	return LEPT_OK;
}

// �ú������ڽ���ͨ�������Ϊ��̬���飬�����elements_count��ʵ�ʳ��ȣ���capability�Ƿ���Ŀ��ö�̬���ȣ����ڵ���elements_count��
// �ú����ڽ������н��ı䵯ջ�����Ϣ���淽�������ռ����뽻����������������ĸ����������append�����г��ֵġ�
// ���践��ͷָ�룬��Ϊ���޸ĵ�ָ��һ���ᱻ���¸�ֵһ�飨������ԭ����ֵ����ע��Ϊ�˱����null��ֵ��ȥ��Ҫ��һ����ʱָ���������ٸ�ֵ��
int lept_set_array_capability(lept_value* value, size_t cap) {
	// ����Ǹ��յ����飬Ҳ����ֱ�ӱ�ralloc������˲��õ����з�֧
	// �����Ҫ��cap��ԭ�еĴ󣬲Ž��д����·��䴦�����򷵻ط���ʧ�ܻ�ֱ�ӷ���ԭֵ������cap���ʱ��
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
	//// Ŀ���ǽ�value�����еĵ�index����ַ�ĳ�target�ĵ�ַ����[]����ȡ������ֱ�Ӿ�������ṹ�屾����Ȼ����������ָ�룩
	//// ��˽������ֵ���ĳ�target�ĵ�ַ���ɴ��Ŀ�ꡣ����������������ͻᵼ��ָ�빲�ã�ͬһ����ַ���������ã����޷���ȷ�ͷš��ĳ�swap��
	//value->u.arr.a[index] = *target;
}

// ����move���������ɹ���ӣ�����ӵ�ָ�뽫���ÿ�
int lept_append_array_elelment(lept_value* value, lept_value* target) {
	// ���capability���㣬��ֱ��move
	if (value->u.arr.capability > value->u.arr.elements_count) {
		lept_move(value->u.arr.a + value->u.arr.elements_count, target);
		value->u.arr.elements_count++;
		return LEPT_OK;
	}
	// ������Ҫʹ��set-capability���������ڴ棬
	else {
		int ret = lept_set_array_capability(value, value->u.arr.capability + ((value->u.arr.capability) >> 1) + 1);  // ͬ��1.5������ԭ�пռ�
		if (ret == LEPT_OK) {
			lept_append_array_elelment(value, target);  // ����ֱ�ӵݹ�append����
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
	// ���ȸ���һ�������ĩβֵ�ظ�append�����飬���󽫵�����һ��ĩβֵð�ݵ�indexλ�á��ǵô������黹�ǿյ�ʱ��
	lept_value* temp_end_element = (lept_value*)malloc(sizeof(lept_value));
	lept_copy(temp_end_element, value->u.arr.a + value->u.arr.elements_count - 1);

	int ret = lept_append_array_elelment(value, temp_end_element);
	lept_value* v1, * v2;
	if (ret == LEPT_OK) {
		// ����Ҫswapԭβindex-Ŀ��index��
		int cur_index;
		for (cur_index = value->u.arr.elements_count - 2; cur_index > index; cur_index) {
			v1 = value->u.arr.a + cur_index;
			v2 = value->u.arr.a + cur_index - 1;
			lept_swap(v1, v2);
		}
		// ��Ŀ��value move��Ŀ��λ��
		lept_move(value->u.arr.a + index, target);
	}
	return ret;
}

size_t lept_remove_array_element(lept_value* value, size_t index) {
	if (index <= value->u.arr.elements_count - 1) {
		// ����������Ŀ��swap��ĩβ
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

// ==============================����======================================
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
		case(LEPT_ARRAY): {  // ������Ҫ��������±�Ҳ����ͬλ
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
	// obj�����������˲������������member���ױ�����������
	size_t cur_len = value->u.obj.members_count;
	size_t cur_cap = value->u.obj.capability;

	// ��������ʱ����ֱ��append��ĩβ
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
	if (del_index == LEPT_OK) {  // ���ҵ���Ҫremove�����ٽ��к�����������ֱ�ӷ��ش�����
		lept_obj_member* temp_mp = value->u.obj.m;

		int cur_index;
		char* temp_char_p;
		size_t temp_klen;
		for (cur_index = (int)del_index; cur_index < value->u.obj.members_count - 1; cur_index++) {

			// ����key��
			temp_char_p = (temp_mp + cur_index)->member_key;
			(temp_mp + cur_index)->member_key = (temp_mp + cur_index + 1)->member_key;
			(temp_mp + cur_index + 1)->member_key = temp_char_p;

			// ����key��
			temp_klen = (temp_mp + cur_index)->key_len;
			(temp_mp + cur_index)->key_len = (temp_mp + cur_index + 1)->key_len;
			(temp_mp + cur_index + 1)->key_len = temp_klen;

			// ����valueֵ
			lept_swap(&(temp_mp + cur_index)->member_value, &(temp_mp + cur_index + 1)->member_value);
		}

		// ��Ԫ������-1,�ͷų���removeԪ�ص�ʹ��Ȩ
		value->u.obj.members_count--;
		return LEPT_OK;
	}
	return del_index;
}
// ========================================================================
// ======================================================================================================================