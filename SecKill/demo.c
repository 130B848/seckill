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

void sort() {
	char t[MAX_LEN];
	float tmp;
	int i, j;
	for (i = 0; i < userNum; i++)
		for (j = i + 1; j < userNum; j++)
			if (strcmp(users[i].id, users[j].id) > 0) {
				// swap users[i] and users[j]
				
				strcpy(t, users[i].id);
				strcpy(users[i].id, users[j].id);
				strcpy(users[j].id, t);

				strcpy(t, users[i].name);
				strcpy(users[i].name, users[j].name);
				strcpy(users[j].name, t);
			}
	for (i = 0; i < commodityNum; i++)
		for (j = i + 1; j < commodityNum; j++)
			if (strcmp(commodities[i].id, commodities[j].id) > 0) {
				// swap commodities[i] and commodities[j]

				strcpy(t, commodities[i].id);
				strcpy(commodities[i].id, commodities[j].id);
				strcpy(commodities[j].id, t);

				strcpy(t, commodities[i].name);
				strcpy(commodities[i].name, commodities[j].name);
				strcpy(commodities[j].name, t);

				tmp = commodities[i].price;
				commodities[i].price = commodities[j].price;
				commodities[j].price = tmp;
			}
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

void check() {
	int i;
	for (i = 0; i < userNum; i++)
		if (id2idx_user(users[i].id) != i) {
			printf("fail at %d\n", i);
			return;
		}
	printf("OK\n");
}

int data_init() {
    FILE *fp;

    if ((fp = fopen("test1000.txt", "r")) == NULL) {
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
        fscanf(fp, "%36[^,],%36[^,],%d,%f\n", commodities[i].id, commodities[i].name, 
                &number, &commodities[i].price);
    }
    
    fclose(fp);
    return 0;
}

int main() {
	data_init();
	sort();
	check();
}