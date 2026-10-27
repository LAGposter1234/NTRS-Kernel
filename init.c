#include <nrts.h>

int main() {
    writeb((char*)0, 'H');
    flushstdout();
    return 0;
}
