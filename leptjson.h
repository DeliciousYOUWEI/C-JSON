// ����궨����ָ����Ϊ���滻�Ƿ�����Ԥ����׶εģ�������ѡ������Ƿ�Ҫ�����¶ν����滻����Ϊ����ظ����õĻ���
// ��Ȼ����û�У���ָ���ͷ�ļ��漰��include�Ļ������������ļ��ظ����ã���ѡ���Ƿ�Ҫ�滻����ע�����������������
// ��ʹ�ý׶�Ҳ��Ҫע��ģ����ǵ������˺�֮������һ���ļ��������η���include���ļ���Ҳ����ͨ������ʹ�ú�ᱨ��
// ��֮��ͷ�ļ��Ļ���������궨������ˡ�
#ifndef LEPTJSON_H__
#define LEPTJSON_H__
#include <stddef.h>

// ����������������͡�
typedef enum { LEPT_NULL, LEPT_TRUE, LEPT_FALSE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

// �������п��ܵĺ����������͡�
typedef enum { LEPT_OK = 1, LEPT_INVALID_VALUE, LEPT_OUT_OF_MEMORY, LEPT_SMALL_THAN_CARRENT_CAP };
# define LEPT_ELEMENT_NOT_EXIST ((size_t)-1)  // ��ȡ��ͬ�豸��size_t��ֵ�ķ�ʽ��ע��size_t�����Ǹ�ֵ���������õľ�����size_t����ķ�ʽ�����ֵ��

// ����һ������Ľṹ�塪�����ﲻʹ����ĸ�������Ϊ�໹���Է�װ��Ϊ������������ݣ�jsonValue����������Ϊ������ýṹ�弴��
typedef struct lept_value lept_value;  // ��Ҫ��һ���ṹ����ڲ�������������Ҫ�ȶ�������ṹ������ƣ���������������ϸ��������
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

// ====================================================ָ�����======================================================
// 
// һ�������ͷ�value�в�������u���ַ��������顢������Ϣ�ĺ��������û���ʹ����prase�����й��ܺ�Ҳ����Ҫʹ�øú����ͷ���Դ��
// ע�������Ҫ�ͷŵ���Դ��Ȼָ�����ַ��������顢������Ϊvalue_pointerָ����ʹ��֮�������䱾��Ϳ����ͷ�������bool���͵���Դ��
// ֻ�в������ȵ���Ϣ�ڸýṹ���д洢����ָ����Ǳ��塣
void lept_free(lept_value* value);

void lept_copy(lept_value* trg_value, const lept_value* src_value);
void lept_move(lept_value* trg_value, lept_value* src_value);
void lept_swap(lept_value* trg_value, lept_value* src_value);
// ==================================================================================================================


// ===========================================�û���ĳ��value��ֵ�����ӿ�===============================================
// �û�����һ�������͵Ľӿڡ�
int lept_init_null(lept_value* value);


//�û�����һ����������value�Ľӿڡ�
int lept_init_boolen(lept_value* value);  // ��ʼ��Ĭ��Ϊtrue
int lept_get_boolen(const lept_value* value);
void lept_set_boolen(lept_value* value, int b);

//�û�����һ��number����value�Ľӿڡ�
int lept_init_number(lept_value* value);  // ��ʼ��Ĭ��Ϊ0
double lept_get_number(const lept_value* value);
void lept_set_number(lept_value* value, double num);

//�û�����һ��string����value�Ľӿڡ�
int lept_init_string(lept_value* value);  // ��ʼ���ַ���ָ��Ĭ��Ϊnull������Ϊ0
char* lept_get_string(const lept_value* value);
size_t lept_get_string_length(const lept_value* value);
void lept_set_string(lept_value* value, char* str, size_t str_len);

//�û�����һ��array����value�Ľӿڡ�
int lept_init_array(lept_value* value);  // ��ʼ������ָ��Ĭ��Ϊnull��Ԫ�ظ���Ϊ0��ע��init������value��ʼ�ռ䣬��ʼ�ռ�Ϊ�գ���Ҫ�û�����������𿪱٣�
int lept_set_array_capability(lept_value* value, size_t cap);  // ��һ����̬�������ռ䣬����lept_ok�������
lept_value* lept_get_array_value(lept_value* value, size_t index);
size_t lept_get_array_elements_count(const lept_value* value);
size_t lept_get_array_capability(const lept_value* value);
size_t lept_set_array_value(lept_value* value, lept_value* target, size_t index);  // ����������ɷ���LEPT_OK�����򷵻�LEPT_ELEMENT_NOT_EXIST������swap���������value������Ҫ�滻��value��
int lept_append_array_elelment(lept_value* value, lept_value* target);  // �ڶ�̬�б��ĩβ����һ��Ԫ�أ��ɹ��򷵻�ok�����򷵻ش�����
int lept_shrink_array(lept_value* value);  // ����̬��������������ͷţ������µ�����ͷָ�롣�ú�����Ӧ�����鳹���޸����ʹ�á�
lept_value* lept_pop_array_elelment(lept_value* value);  // �������һ��value�������ظ�value��ָ�루��c����֧�ֺ������أ������Ҫһ��ר�ŵĺ�����index��
int lept_insert_array(lept_value* value, lept_value* target, size_t index);  // ��array��ָ��λ�ò���һ��ֵ��������swapð��ʵ�֣�
size_t lept_remove_array_element(lept_value* value, size_t index);


//�û�����һ��obj����value�Ľӿڡ�
int lept_init_obj(lept_value* value);  // ��ʼ�������Ա����ָ��Ĭ��Ϊnull����Ա����Ϊ0���ú���ͬ���������ʼ�ռ�
int lept_set_obj_capability(lept_value* value, size_t cap);  // ��һ����̬�������ռ䣬����lept_ok�������
size_t lept_get_obj_members_count(lept_value* value);
const char* lept_get_obj_key(lept_value* value, size_t index);
size_t lept_get_obj_key_len(lept_value* value, size_t index);
lept_value* lept_get_obj_value(lept_value* value, size_t index);
size_t lept_find_obj_key(const lept_value* value, const char* target_key, size_t tklen);
lept_value* lept_find_obj_member(const lept_value* value, const char* target_key, size_t tklen);
int lept_is_equal(const lept_value* src_value, const lept_value* trg_value);
size_t lept_set_obj_value(lept_value* trg_obj, lept_obj_member* src_member);  // ����LEPT_OK�����򷵻�LEPT_OBJ_KEY_NOT_EXIST������swap���������value������Ҫ�滻��value��
int lept_append_obj_member(lept_value* value, const char* target_key, size_t klen, lept_value* src_member_value);
int lept_shrink_obj(lept_value* value);
size_t lept_remove_obj_member(lept_value* value, const char* target_key, size_t klen);

// =====================================================================================================================
#endif