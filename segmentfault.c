#include <stdio.h>

int main() {
	char *str = NULL;
	str = (char*) malloc(500000000);
	//free(str);
	//free(str+1);
	//*str = 'a';
	//printf("%s\n", str);
	return 0;
}
