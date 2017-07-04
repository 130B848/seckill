#include <stdio.h>
#include <string.h>

#define MAX_LEN 40
#define MAX_NUM 1000

static unsigned int userNum, commodityNum;

struct userInfo {
    char id[MAX_LEN];
    char name[MAX_LEN];
};
typedef struct userInfo user_t;
static user_t users[MAX_NUM];

struct commodityInfo {
    char id[MAX_LEN];
    char name[MAX_LEN];
    float price;
};
typedef struct commodityInfo commodity_t;
static commodity_t commodities[MAX_NUM];

void swap(char *x, char *y) {
	char t[MAX_LEN];
	strcpy(t, x);
	strcpy(x, y);
	strcpy(y, t);
}

void sort() {
	int i, j;
	for (i = 0; i < userNum; i++)
		for (j = i + 1; j < userNum; j++)
			if (strcmp(users[i].id, users[j].id) > 0) {
				// swap users[i] and users[j]
				swap(users[i].id, users[j].id);
				swap(users[i].name, users[j].name);
			}
	for (i = 0; i < commodityNum; i++)
		for (j = i + 1; j < commodityNum; j++)
			if (strcmp(commodities[i].id, commodities[j].id) > 0) {
				// swap commodities[i] and commodities[j]
				swap(commodities[i].id, commodities[j].id);
				swap(commodities[i].name, commodities[j].name);

				float tmp = commodities[i].price;
				commodities[i].price = commodities[j].price;
				commodities[j].price = tmp;
			}
}

void print() {
	int i;
	for (i = 0; i < userNum; i++)
		printf("%s, %s\n", users[i].id, users[i].name);
	for (i = 0; i < commodityNum; i++)
		printf("%s, %s, %f\n", commodities[i].id, commodities[i].name, commodities[i].price);
}

void check_sort() {
	int i, j, flag = 0;
	for (i = 0; i < userNum; i++)
		for (j = i + 1; j < userNum; j++)
			if (strcmp(users[i].id, users[j].id) > 0) {
				flag = 1;
				break;
			}
	if (flag) 
		printf("users sort FAILED\n");
	else
		printf("users sort OK\n");

	flag = 0;
	for (i = 0; i < commodityNum; i++)
		for (j = i + 1; j < commodityNum; j++)
			if (strcmp(commodities[i].id, commodities[j].id) > 0) {
				flag = 1;
				break;
			}
	if (flag) 
		printf("commodities sort FAILED\n");
	else
		printf("commodities sort OK\n");
}

int id2idx_user(char *id) {
	int l = 0, r = userNum - 1;
	while (l <= r) {
		int mid = (l + r) >> 1;
		int d = strcmp(id, users[mid].id);
		if (d > 0)
			l = mid + 1;
		else if (d < 0)
			r = mid - 1;
		else return mid;
	}
	return -1;
}

int id2idx_cmdt(char *id) {
	int l = 0, r = commodityNum - 1;
	while (l <= r) {
		int mid = (l + r) >> 1;
		int d = strcmp(id, commodities[mid].id);
		if (d > 0)
			l = mid + 1;
		else if (d < 0)
			r = mid - 1;
		else return mid;
	}
	return -1;
}

void check_binary_search() {
	int i;
	for (i = 0; i < userNum; i++)
		if (id2idx_user(users[i].id) != i) {
			printf("fail at %d\n", i);
			return;
		}
	printf("users binary search OK\n");

	for (i = 0; i < commodityNum; i++)
		if (id2idx_cmdt(commodities[i].id) != i) {
			printf("fail at %d\n", i);
			return;
		}
	printf("commodities binary search OK\n");
}

int data_init() {
    FILE *fp;

    if ((fp = fopen("input.txt", "r")) == NULL) {
        return -1;
    }

    size_t i;
    
    fscanf(fp, "%u\n", &userNum);
    memset(users, 0, sizeof(user_t) * MAX_NUM);
    float balance;
    for (i = 0; i < userNum; i++) {
        fscanf(fp, "%36[^,],%36[^,],%f\n", users[i].id, users[i].name, &balance);
    }
    
    fscanf(fp, "%u\n", &commodityNum);
    memset(commodities, 0, sizeof(commodity_t) * MAX_NUM);
    int number;
    for (i = 0; i < commodityNum; i++) {
        fscanf(fp, "%36[^,],%36[^,],%f,%d\n", commodities[i].id, commodities[i].name, 
                &commodities[i].price, &number);
    }
    
    fclose(fp);
    return 0;
}

int main() {
	data_init();
	sort();
	check_sort();
	check_binary_search();
	print();
}