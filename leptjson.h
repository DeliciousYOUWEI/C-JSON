// ����궨����ָ����Ϊ���滻�Ƿ�����Ԥ����׶εģ�������ѡ������Ƿ�Ҫ�����¶ν����滻����Ϊ����ظ����õĻ���
// ��Ȼ����û�У���ָ���ͷ�ļ��漰��include�Ļ������������ļ��ظ����ã���ѡ���Ƿ�Ҫ�滻����ע�����������������
// ��ʹ�ý׶�Ҳ��Ҫע��ģ����ǵ������˺�֮������һ���ļ��������η���include���ļ���Ҳ����ͨ������ʹ�ú�ᱨ��
// ��֮��ͷ�ļ��Ļ���������궨������ˡ�
#ifndef LEPTJSON_H__
#define LEPTJSON_H__
#include <stddef.h>


// ����������������͡�
typedef enum{LEPT_NULL, LEPT_TRUE, LEPT_FALSE, LEPT_STRING, LEPT_NUMBER, LEPT_ARRAY, LEPT_OBJECT} lept_type;

// �������п��ܵĺ����������͡�
typedef enum{LEPT_OK = 1, LEPT_INVALID_VALUE};

// ����һ������Ľṹ�塪�����ﲻʹ����ĸ�������Ϊ�໹���Է�װ��Ϊ������������ݣ�jsonValue����������Ϊ������ýṹ�弴��
typedef struct lept_value lept_value;  // ��Ҫ��һ���ṹ����ڲ�������������Ҫ�ȶ�������ṹ������ƣ���������������ϸ��������
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

// һ�������ͷ�value�в�������u���ַ��������顢������Ϣ�ĺ��������û���ʹ����prase�����й��ܺ�Ҳ����Ҫʹ�øú����ͷ���Դ��
// ע�������Ҫ�ͷŵ���Դ��Ȼָ�����ַ��������顢������Ϊvalue_pointerָ����ʹ��֮�������䱾��Ϳ����ͷ�������bool���͵���Դ��
// ֻ�в������ȵ���Ϣ�ڸýṹ���д洢����ָ����Ǳ��塣
void lept_free(lept_value* value);

//�û�����һ����������value�Ľӿڡ�
int lept_get_boolen(const lept_value* value);
void lept_set_boolen(lept_value* value, int b);

//�û�����һ��number����value�Ľӿڡ�
double lept_get_number(const lept_value* value);
void lept_set_number(lept_value* value, double num);

//�û�����һ��string����value�Ľӿڡ�
char* lept_get_string(const lept_value* value);
size_t lept_get_string_length(const lept_value* value);
void lept_set_string(lept_value* value, char* str, size_t str_len);

//�û�����һ��array����value�Ľӿڡ�
lept_value lept_get_array_value(lept_value* value, size_t index);
size_t lept_get_array_length(const lept_value* value);
void lept_set_arrray_value(lept_value* value, lept_value* target, size_t index);

//�û�����һ��obj����value�Ľӿڡ�����ȡ�����ö���ĳ�Ա������ȡ�����ö���ĳ�Աֵ��������Ҫʲô������


#endif