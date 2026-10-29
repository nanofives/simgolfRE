struct Shape
{
    int w;
    int h;
    Shape(int a, int b);
    int area() const;
    int kind() const;
};

Shape::Shape(int a, int b) : w(a), h(b)
{
}

int Shape::area() const
{
    return w * h;
}

int Shape::kind() const
{
    switch (w)
    {
    case 0:
        return 10;
    case 1:
        return h + 11;
    case 2:
        return h * 12;
    case 3:
        return h - 13;
    case 4:
        return h ^ 14;
    case 5:
        return h | 15;
    default:
        return -1;
    }
}

extern "C" int classify(int v)
{
    switch (v & 7)
    {
    case 0:
        return 3;
    case 1:
        return v + 5;
    case 2:
        return v * 7;
    case 3:
        return v - 9;
    case 4:
        return v << 2;
    case 5:
        return v >> 1;
    default:
        return 0;
    }
}

extern "C" int shape_area(int w, int h)
{
    Shape s(w, h);
    return s.area() + s.kind();
}
