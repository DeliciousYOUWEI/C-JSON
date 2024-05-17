#include "leptjson.h"
#include<stdio.h>
#include<malloc.h>

void main() {

	char Json_context[] = "   [\"haoyouwei\", 114, 514, [\"N\", \"B\"], true]";
	char* inJson = Json_context;

	lept_value output_value;
	int ret;
	ret = lept_prase(&output_value, inJson);
	lept_type output_type = lept_get_type(&output_value);
	printf("0");
}