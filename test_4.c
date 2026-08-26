typedef int* IntPtr;

int main() {
    IntPtr p1;
    int *p2;

    BadType bad_var;

    p1 = 0;
    p2 = p1;

    return 0;
}