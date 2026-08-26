typedef int Integer;
typedef float Real;

int main() {
    // 1. Valid declarations using C standard types and typedefs
    Integer a, b;
    Real final_score;
    int counter;

    // 2. Errors: Invalid variable names (starting with numbers or illegal symbols)
    int value123;
    float $price;

    // 3. Errors: Invalid or undefined data types
    UnknownType x;
    string_t message;

    // 4. Error: Declared variable that is never used
    int unused_var;

    /* Usage of variables to mark them as used */
    a = 10;
    b = a + 5;
    final_score = 28.5f;
    counter = b * 2;

    return 0;
}