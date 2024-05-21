#include "leptjson.h"
#include<stdio.h>
#include<malloc.h>

void main() {

	char Json_context[] = "   [\"haoyouwei\", 114, 514, [\"N\", \"B\"], true]";
	char* inJson = Json_context;
	//size_t temp_size = sizeof(Json_context);
	char* temp_char = (char*)malloc(sizeof(Json_context));
	*temp_char = Json_context;
	temp_char[44] = '\0';
	
	//char* test = (char*)malloc(sizeof(Json_context));

	char new_json[] = "\"man what can I say\"";
	char* newJson = new_json;

	lept_value output_value, new_value;
	int ret;
	ret = lept_prase(&output_value, inJson);
	ret = lept_prase(&new_value, newJson);

	//// 1.指针列表直接+1即可偏移，无需+特定size(在栈中之所以要移动sizeof(lept_vlue)是因为栈中的指针列表是char类型)
	//// 而这里是lept_value，自然含有长度信息。
	//lept_value* temp = output_value.u.arr.a+4;

	//lept_value* ori_target = &new_value;
	lept_value* ori_target = output_value.u.arr.a + 1;
	lept_value* ori_source = output_value.u.arr.a + 2;  // 只要是从栈中整套复制过来的，就都是连续空间（不是1字节范围的连续，而是类型长度尺度的连续）
	size_t temp_len = sizeof(lept_value);

	lept_set_arrray_value(&output_value, &new_value, 2);

	lept_value* temp = output_value.u.arr.a + 2;
	

	lept_type output_type = lept_get_type(&output_value);
	printf("0");
}