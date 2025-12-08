#include <stdio.h>

static int story_num = 0;


void Story_selif(int story) {
	if(story == 0){
		printf("\n***********************\n\n");
	}else if(story == 1){
		printf("\n\n***********************");
	}
}
void Story_selif2(char *str) {
	printf("\n***********************\n\n");
	printf("%s",str);
	printf("\n\n***********************");
}

