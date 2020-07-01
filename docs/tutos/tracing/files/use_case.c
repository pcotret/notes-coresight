#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int main(void){
        char buffer[20];
        FILE *fs;
        // geteuid() is considered "secure"
        if(geteuid() != 0){ // user
                fs = fopen("welcome", "r");      // Opening a public file.
                if(!fs) return -1;
        }
        else{ // root
                fs = fopen("passwd", "r");     // Opening a secret file.
                if(!fs) return -2;
        }
        fread(buffer, 1, sizeof(buffer), fs);
        fclose(fs);
        printf("Buffer Value : %s \n", buffer);
        return 0;
}
