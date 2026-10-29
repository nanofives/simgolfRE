

struct Guard {
    Guard();
    ~Guard();
};
void work(int);

int eh(int v)
{
    try {
        Guard g;
        work(v);
    } catch (int e) {
        return e;
    }
    return 0;
}













