#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
  int i;
  int ret;
  char buf[8];

  if ((ret = read(0, buf, 8)) < 1) {
    printf("Hum?\n");
    exit(1);
  }

  if (buf[0] == '0'){
    printf("Looks like a zero to me!\n");
  }else if(buf[0] == '1'){
    for(i = 0;i < 10;i++)
      printf("Looks like a one to me!\n");
  }else{
	int loopcnt;
	if(ret >= 4){
	  loopcnt = buf[0] + buf[1] - buf[2] + buf[3];
	  printf("buf[%d]: %d\t\tbuf[%d]: %d\t\tbuf[%d]: %d\t\tbuf[%d]: %d\n", 0, buf[0], 1, buf[1], 2, buf[2], 3, buf[3]);
	}else if(ret == 3){
	  loopcnt = buf[0] + buf[1] - buf[2];
	  printf("buf[%d]: %d\t\tbuf[%d]: %d\t\tbuf[%d]: %d\n", 0, buf[0], 1, buf[1], 2, buf[2]);
	}else if(ret == 2){
	  loopcnt = buf[0] + buf[1];
	  printf("buf[%d]: %d\t\tbuf[%d]: %d\n", 0, buf[0], 1, buf[1]);
	}else if(ret == 1){
	  loopcnt = buf[0];
	  printf("buf[%d]: %d\n", 0, buf[0]);
	}
	printf("loopcnt: %d\n", loopcnt);
	for(i = 0;i < loopcnt;i++){
      printf("A else value? How quaint!\n");
    }
  }
  exit(0);

}
