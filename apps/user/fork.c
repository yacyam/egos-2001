#include "app.h"

int main() {
    INFO("One");
    int pid = fork();
    SUCCESS("Two %d", pid);
}