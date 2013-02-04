#include <stdio.h>

int test(char** p);
int test1(char** p);
#if 0
int main() {
	char* p = NULL;
	printf("address of p : %x\n", &p);
	//printf("value of p : %x\n", *p);

	test(&p);

	printf("main %c %c %c \n", p[0], p[1], p[2]);
	return 0;
}

int test(char** p) {
	char* inner;
	inner = (char*) malloc (3);
	inner[0] = 'a';
	inner[1] = 'b';
	inner[2] = 'c';
	printf("test: %c\n", inner[0]);
	*p = inner;	
	printf("test: %c\n", *p[0]);
	return 0;
}
#endif
int main() {
	char* p = NULL;
	printf("000 %x\n", &p);
	test(&p);
	//printf("-->%c\n", p[0]);
	return 0;
}

int test(char** p) {
	test1(p);
	printf("1111 %x\n", p);
	return 0;
}
int test1(char** p) {
	printf("address %x\n", p);
	*p = (char*) malloc(4);
	(*p)[0] = 'a';
	return 0;
}
