#include <unistd.h>

int main(void) {
        if (acct("acct_test.txt") == -1) {
                perror("acct");
        }
       
        printf("sadqweq");
        sleep(1);
        for (int i = 0; i < 1 << 18; ++i);

        return 0;
}