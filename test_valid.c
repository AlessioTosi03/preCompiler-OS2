typedef double Time;

int main() {
    int i, sum;
    Time start_time;

    sum = 0;
    start_time = 0.0;

    for (i = 0; i < 10; ++i) {
        sum += i;
    }

    start_time = sum * 1.5;

    return 0;
}