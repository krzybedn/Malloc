/*
	Krzysztof Bednarek
	292974
	Systemy operacyjne (zaawansowane)
	pracownia 3
*/

#include "malloc.h"
#include "minunit.h"

void* test1;
void* test2;
void* test3;
void* test4;
void* test5;
void* test6;

void test_setup(void)
{
	test1 = my_malloc(sizeof(int));//sprawdzamy, czy na pewno nie przenosimy bez potrzeby podczas calloca
	test2 = my_realloc(test1, sizeof(char));

	test3 = my_malloc(1);//alokujemy maly obszar, by sprawdzic wyrowanieni adresow

	test4 = my_calloc(8, sizeof(char));//sprawdzamy, czy calloc faktycznie wyzerowal podany obszar

	test5 = my_malloc(sizeof(int));
	*((int*)test5) = 123456789;
	test6 = my_realloc(test5, 10000);//alokujemy duzo miejsca, aby zwiekszyc szanse, ze obszar zostal przeniesiony
										//nastepnie sprawdzimy, czy zostala skopiowana wartosc

}

void test_teardown(void) {
	my_free(test1);
	my_free(test2);
	my_free(test3);
	my_free(test4);
	my_free(test5);
	my_free(test6);
}

MU_TEST(alligned)
{
	mu_check((int64_t)test1 % 8 == 0);
	mu_check((int64_t)test2 % 8 == 0);
	mu_check((int64_t)test3 % 8 == 0);
	mu_check((int64_t)test4 % 8 == 0);
	mu_check((int64_t)test5 % 8 == 0);
	mu_check((int64_t)test6 % 8 == 0);
}

MU_TEST(reducing_realloc)
{
	mu_check(test1 == test2);
}

MU_TEST(increasing_realloc)
{
	mu_check(((int*)test6)[0] == 123456789);
}

MU_TEST(calloc_clear_memory)
{
	mu_check(*((long long int*)test4) == 0);
}

MU_TEST_SUITE(test_suite)
{
	MU_SUITE_CONFIGURE(&test_setup, &test_teardown);
	MU_RUN_TEST(alligned);
	MU_RUN_TEST(reducing_realloc);
	MU_RUN_TEST(increasing_realloc);
	MU_RUN_TEST(calloc_clear_memory);
}

int main()
{
	MU_RUN_SUITE(test_suite);
	MU_REPORT();
	int32_t cos = 123456789;
	size_t cos2 = cos;
	printf("%lu\n", cos2);
	return 0;
}
