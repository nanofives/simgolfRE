// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll math library (debug build, C++ exception handling on): Vector3 {float v[3]}, Quat {Vector3 v; float w},
// Matrix {vptr; union { float m[16]; float e[4][4]; }} (column-major: translation in m[12..14]) and Transform
// {vptr; Quat q; Vector3 p; unsigned flags} (flags 2 = translation set, 0x1c = rotation set). Class and member
// names are chosen here; each // MATCH: is a 100% instruction match.
// Compiler facts these functions pin down: the Matrix and Transform copy constructors (0x10006720, 0x100068d0)
// and Transform::operator= (0x10006840) are compiler-generated (the Matrix one copies both union members);
// VC6 /Od compares a double with 1.0 as two integer compares (Vector3::normalize); `if (x) a; else { b; }`
// and the inverted form compile differently, and an `|= 2` on an unsigned member uses a full register.
// Transform::translate(const Vector3&) (0x10005990) and Transform::move (0x10005c70) have identical bodies.
#include <math.h>
class Quat;
class Vector3 {
public:
    void set(float x, float y, float z);
    void add(float x, float y, float z);
    bool operator==(const Vector3& o);
    Vector3 operator+(const Vector3& o) const;
    Vector3 operator*(float s) const;
    Vector3& operator*=(float s);
    Vector3& operator+=(const Vector3& o);
    Vector3& operator-=(const Vector3& o);
    float distance(const Vector3& o);
    float distance2(const Vector3& o);
    float dot(const Vector3& o) const;
    void normalize();
    Vector3 cross(const Vector3& o) const;
    void rotate(const Quat* q);
    float v[3];
};
class Quat {
public:
    Quat();
    void identity();
    Quat operator*(const Quat& q);
    Quat& operator*=(const Quat& q);
    void conjugate();
    void setAxisAngle(float angle, const Vector3& axis);
    Vector3 v;
    float w;
};
class Matrix {
public:
    Matrix();
    virtual ~Matrix();
    void identity();
    void reset();
    Matrix operator+(const Matrix& o);
    Matrix& operator*=(const Matrix& o);
    Matrix operator*(float s);
    Matrix operator*(const Matrix& o);
    bool operator==(const Matrix& o);
    Vector3 operator*(const Vector3& p);
    void setRotation(const Quat& q);
    Matrix& translate(const Vector3& d);
    Matrix& translate(float x, float y, float z);
    union { float m[16]; float e[4][4]; };
};
class Transform {
public:
    Transform();
    virtual ~Transform();
    void reset();
    void assign(const Quat& q, const Vector3& p);
    void setTranslation(const Vector3& p);
    void setRotation(const Quat& q);
    void set(const Quat& q, const Vector3& p);
    void getMatrix(Matrix& M);
    Transform& translate(const Vector3& d);
    Transform& untranslate(const Vector3& d);
    Transform& translate(float x, float y, float z);
    Transform& move(const Vector3& d);
    Vector3 apply(const Vector3& v);
    Transform operator+(const Vector3& d);
    Transform operator-(const Vector3& d);
    Transform operator*(const Transform& o);
    Transform& operator*=(const Transform& o);
    Transform scaled(float s);
    void rotate(float angle, const Vector3& axis);
    void rotateX(float angle);
    void rotateY(float angle);
    void rotateZ(float angle);
    Quat q;
    Vector3 p;
    unsigned int flags;
};

// MATCH: jgld.dll 0x100032f0 ??1Matrix@@UAE@XZ
Matrix::~Matrix() { identity(); }

// MATCH: jgld.dll 0x100034e0 ??1Transform@@UAE@XZ
Transform::~Transform() { reset(); }

// MATCH: jgld.dll 0x10003530 ?assign@Transform@@QAEXABVQuat@@ABVVector3@@@Z
void Transform::assign(const Quat& q, const Vector3& p)
{
    this->q = q;
    this->p = p;
}

// MATCH: jgld.dll 0x100039b0 ??DVector3@@QBE?AV0@M@Z
Vector3 Vector3::operator*(float s) const
{
    Vector3 r;
    for (int i = 0; i < 3; i++)
        r.v[i] = s * v[i];
    return r;
}

// MATCH: jgld.dll 0x10003a40 ??XVector3@@QAEAAV0@M@Z
Vector3& Vector3::operator*=(float s)
{
    for (int i = 0; i < 3; i++)
        v[i] = s * v[i];
    return *this;
}

// MATCH: jgld.dll 0x10003ab0 ??8Vector3@@QAE_NABV0@@Z
bool Vector3::operator==(const Vector3& o)
{
    if (v[0] == o.v[0] && v[1] == o.v[1] && v[2] == o.v[2])
        return true;
    return false;
}

// MATCH: jgld.dll 0x10003b30 ?distance@Vector3@@QAEMABV1@@Z
float Vector3::distance(const Vector3& o)
{
    double s = 0.0;
    for (int i = 0; i < 3; i++)
        s += (v[i] - o.v[i]) * (v[i] - o.v[i]);
    s = sqrt(s);
    float r = (float)s;
    return r;
}

// MATCH: jgld.dll 0x10003bf0 ?distance2@Vector3@@QAEMABV1@@Z
float Vector3::distance2(const Vector3& o)
{
    float s = 0.0f;
    for (int i = 0; i < 3; i++)
        s += (v[i] - o.v[i]) * (v[i] - o.v[i]);
    return s;
}

// MATCH: jgld.dll 0x10003c90 ?dot@Vector3@@QBEMABV1@@Z
float Vector3::dot(const Vector3& o) const
{
    return v[0] * o.v[0] + v[1] * o.v[1] + v[2] * o.v[2];
}

// MATCH: jgld.dll 0x10003cf0 ?normalize@Vector3@@QAEXXZ
void Vector3::normalize()
{
    double len = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    len = sqrt(len);
    if (len == 0.0 || len == 1.0)
        return;
    len = 1.0 / len;
    v[0] *= len;
    v[1] *= len;
    v[2] *= len;
}

// MATCH: jgld.dll 0x10003e50 ?cross@Vector3@@QBE?AV1@ABV1@@Z
Vector3 Vector3::cross(const Vector3& o) const
{
    Vector3 r;
    r.v[0] = v[1] * o.v[2] - v[2] * o.v[1];
    r.v[1] = -v[0] * o.v[2] + v[2] * o.v[0];
    r.v[2] = v[0] * o.v[1] - v[1] * o.v[0];
    return r;
}

// MATCH: jgld.dll 0x10004100 ?rotate@Vector3@@QAEXPBVQuat@@@Z
void Vector3::rotate(const Quat* q)
{
    Quat p;
    Quat c;
    if (q == 0)
        return;
    c = *q;
    p.w = 0.0f;
    p.v = *this;
    p = c * p;
    c.conjugate();
    p = p * c;
    *this = p.v;
}

// MATCH: jgld.dll 0x10004230 ?setAxisAngle@Quat@@QAEXMABVVector3@@@Z
void Quat::setAxisAngle(float angle, const Vector3& axis)
{
    float half, c, s;
    c = (float)cos(half = 0.5f * angle * 0.017453292f);
    s = (float)sin(half);
    w = c;
    v = axis;
    v *= s;
}

// MATCH: jgld.dll 0x100042f0 ??XQuat@@QAEAAV0@ABV0@@Z
Quat& Quat::operator*=(const Quat& q)
{
    Quat r;
    r.w = w * q.w - v.dot(q.v);
    r.v = v.cross(q.v) + q.v * w + v * q.w;
    *this = r;
    return *this;
}

// MATCH: jgld.dll 0x10004410 ??DQuat@@QAE?AV0@ABV0@@Z
Quat Quat::operator*(const Quat& q)
{
    Quat r;
    r.w = w * q.w - v.dot(q.v);
    r.v = v.cross(q.v) + q.v * w + v * q.w;
    return r;
}

// MATCH: jgld.dll 0x10004530 ?conjugate@Quat@@QAEXXZ
void Quat::conjugate()
{
    v.v[0] = -v.v[0];
    v.v[1] = -v.v[1];
    v.v[2] = -v.v[2];
}

// MATCH: jgld.dll 0x10004590 ??0Matrix@@QAE@XZ
Matrix::Matrix() { identity(); }

// MATCH: jgld.dll 0x10004750 ?setRotation@Matrix@@QAEXABVQuat@@@Z
void Matrix::setRotation(const Quat& q)
{
    float wx, wy, wz, xx, xy, xz, yy, yz, zz, x2, y2, z2;
    x2 = q.v.v[0] + q.v.v[0];
    y2 = q.v.v[1] + q.v.v[1];
    z2 = q.v.v[2] + q.v.v[2];
    xx = x2 * q.v.v[0];
    xy = y2 * q.v.v[0];
    xz = z2 * q.v.v[0];
    yy = y2 * q.v.v[1];
    yz = z2 * q.v.v[1];
    zz = z2 * q.v.v[2];
    wx = x2 * q.w;
    wy = y2 * q.w;
    wz = z2 * q.w;
    m[0] = 1.0f - (yy + zz);
    m[4] = xy - wz;
    m[8] = xz + wy;
    m[1] = xy + wz;
    m[5] = 1.0f - (xx + zz);
    m[9] = yz - wx;
    m[2] = xz - wy;
    m[6] = yz + wx;
    m[10] = 1.0f - (xx + yy);
}

// MATCH: jgld.dll 0x100048e0 ??HMatrix@@QAE?AV0@ABV0@@Z
Matrix Matrix::operator+(const Matrix& o)
{
    Matrix r;
    for (int i = 0; i < 16; i++)
        r.m[i] = m[i] + o.m[i];
    return r;
}

// MATCH: jgld.dll 0x10004a50 ??XMatrix@@QAEAAV0@ABV0@@Z
Matrix& Matrix::operator*=(const Matrix& o)
{
    Matrix r;
    int i;
    int k = 0;
    for (i = 0; i < 4; i++) {
        r.m[k] = m[k] * o.m[0] + m[k + 1] * o.m[4] + m[k + 2] * o.m[8] + m[k + 3] * o.m[12];
        r.m[k + 1] = m[k] * o.m[1] + m[k + 1] * o.m[5] + m[k + 2] * o.m[9] + m[k + 3] * o.m[13];
        r.m[k + 2] = m[k] * o.m[2] + m[k + 1] * o.m[6] + m[k + 2] * o.m[10] + m[k + 3] * o.m[14];
        r.m[k + 3] = m[k] * o.m[3] + m[k + 1] * o.m[7] + m[k + 2] * o.m[11] + m[k + 3] * o.m[15];
        k += 4;
    }
    *this = r;
    return *this;
}

// MATCH: jgld.dll 0x10004dc0 ??8Matrix@@QAE_NABV0@@Z
bool Matrix::operator==(const Matrix& o)
{
    for (int i = 0; i < 16; i++)
        if (m[i] != o.m[i])
            return false;
    return true;
}

// MATCH: jgld.dll 0x10004e40 ??DMatrix@@QAE?AVVector3@@ABV1@@Z
Vector3 Matrix::operator*(const Vector3& p)
{
    Vector3 r;
    r.v[0] = m[0] * p.v[0] + m[4] * p.v[1] + m[8] * p.v[2] + m[12];
    r.v[1] = m[1] * p.v[0] + m[5] * p.v[1] + m[9] * p.v[2] + m[13];
    r.v[2] = m[2] * p.v[0] + m[6] * p.v[1] + m[10] * p.v[2] + m[14];
    return r;
}

// MATCH: jgld.dll 0x10005360 ?translate@Matrix@@QAEAAV1@ABVVector3@@@Z
Matrix& Matrix::translate(const Vector3& d)
{
    m[12] += d.v[0];
    m[13] += d.v[1];
    m[14] += d.v[2];
    return *this;
}

// MATCH: jgld.dll 0x100053e0 ?translate@Matrix@@QAEAAV1@MMM@Z
Matrix& Matrix::translate(float x, float y, float z)
{
    m[12] = x + m[12];
    m[13] = y + m[13];
    m[14] = z + m[14];
    return *this;
}

// MATCH: jgld.dll 0x10005450 ??0Transform@@QAE@XZ
Transform::Transform() { reset(); }

// MATCH: jgld.dll 0x100054b0 ?setTranslation@Transform@@QAEXABVVector3@@@Z
void Transform::setTranslation(const Vector3& p)
{
    flags = 2;
    q.identity();
    this->p = p;
}

// MATCH: jgld.dll 0x10005530 ?setRotation@Transform@@QAEXABVQuat@@@Z
void Transform::setRotation(const Quat& q)
{
    flags = 0x1c;
    this->q = q;
    p.set(0.0f, 0.0f, 0.0f);
}

// MATCH: jgld.dll 0x100055c0 ?set@Transform@@QAEXABVQuat@@ABVVector3@@@Z
void Transform::set(const Quat& q, const Vector3& p)
{
    flags = 0x1e;
    this->q = q;
    this->p = p;
}

// MATCH: jgld.dll 0x10005650 ?reset@Transform@@QAEXXZ
void Transform::reset() { flags = 0; }

// MATCH: jgld.dll 0x10005690 ?getMatrix@Transform@@QAEXAAVMatrix@@@Z
void Transform::getMatrix(Matrix& M)
{
    float wx, wy, wz, xx, xy, xz, yy, yz, zz, x2, y2, z2;
    M.reset();
    if (flags & 0x1c) {
        x2 = q.v.v[0] + q.v.v[0];
        y2 = q.v.v[1] + q.v.v[1];
        z2 = q.v.v[2] + q.v.v[2];
        xx = x2 * q.v.v[0];
        xy = y2 * q.v.v[0];
        xz = z2 * q.v.v[0];
        yy = y2 * q.v.v[1];
        yz = z2 * q.v.v[1];
        zz = z2 * q.v.v[2];
        wx = x2 * q.w;
        wy = y2 * q.w;
        wz = z2 * q.w;
        M.m[0] = 1.0f - (yy + zz);
        M.m[4] = xy - wz;
        M.m[8] = xz + wy;
        M.m[1] = xy + wz;
        M.m[5] = 1.0f - (xx + zz);
        M.m[9] = yz - wx;
        M.m[2] = xz - wy;
        M.m[6] = yz + wx;
        M.m[10] = 1.0f - (xx + yy);
    }
    if (flags & 2) {
        M.m[12] = p.v[0];
        M.m[13] = p.v[1];
        M.m[14] = p.v[2];
    }
}

// MATCH: jgld.dll 0x10005890 ??HTransform@@QAE?AV0@ABVVector3@@@Z
Transform Transform::operator+(const Vector3& d)
{
    Transform t;
    t = *this;
    if (t.flags & 2)
        t.p += d;
    else {
        t.flags |= 2;
        t.p = d;
    }
    return t;
}

// MATCH: jgld.dll 0x10005990 ?translate@Transform@@QAEAAV1@ABVVector3@@@Z   // 0x10005c70 compiles identically
Transform& Transform::translate(const Vector3& d)
{
    if (flags & 2)
        p += d;
    else {
        flags |= 2;
        p = d;
    }
    return *this;
}

// MATCH: jgld.dll 0x10005a30 ??GTransform@@QAE?AV0@ABVVector3@@@Z
Transform Transform::operator-(const Vector3& d)
{
    Transform t;
    t = *this;
    if (t.flags & 2)
        t.p -= d;
    else {
        t.flags |= 2;
        t.p = d;
    }
    return t;
}

// MATCH: jgld.dll 0x10005b30 ?untranslate@Transform@@QAEAAV1@ABVVector3@@@Z
Transform& Transform::untranslate(const Vector3& d)
{
    if (flags & 2)
        p -= d;
    else {
        flags |= 2;
        p = d;
    }
    return *this;
}

// MATCH: jgld.dll 0x10005bd0 ?translate@Transform@@QAEAAV1@MMM@Z
Transform& Transform::translate(float x, float y, float z)
{
    if (flags & 2)
        p.add(x, y, z);
    else {
        flags |= 2;
        p.set(x, y, z);
    }
    return *this;
}

// MATCH: jgld.dll 0x10005c70 ?move@Transform@@QAEAAV1@ABVVector3@@@Z   // same body as translate (0x10005990): name not determined
Transform& Transform::move(const Vector3& d)
{
    if (flags & 2)
        p += d;
    else {
        flags |= 2;
        p = d;
    }
    return *this;
}

// MATCH: jgld.dll 0x10005d10 ?apply@Transform@@QAE?AVVector3@@ABV2@@Z
Vector3 Transform::apply(const Vector3& v)
{
    Vector3 r = v;
    if (flags & 0x1c)
        r.rotate(&q);
    if (flags & 2)
        r += p;
    return r;
}

// MATCH: jgld.dll 0x10006720 ??0Matrix@@QAE@ABV0@@Z

// MATCH: jgld.dll 0x10006840 ??4Transform@@QAEAAV0@ABV0@@Z

// MATCH: jgld.dll 0x100068d0 ??0Transform@@QAE@ABV0@@Z
// MATCH: jgld.dll 0x10005dd0 ??DTransform@@QAE?AV0@ABV0@@Z
Transform Transform::operator*(const Transform& o)
{
    Transform t;
    if (flags == 0 || o.flags == 0) {
        if (o.flags)
            t = o;
        else if (flags == 0)
            t = *this;
    } else {
        t = *this;
        if (o.flags & 0x1c) {
            if (flags & 2)
                t.p.rotate(&o.q);
            if (flags & 0x1c)
                t.q = q * o.q;
            else
                t.q = q;
        }
        if (o.flags & 2) {
            if (t.flags & 2)
                t.p += o.p;
            else
                t.p = o.p;
        }
    }
    t.flags = flags | o.flags;
    return t;
}
// MATCH: jgld.dll 0x10005ff0 ?scaled@Transform@@QAE?AV1@M@Z
Transform Transform::scaled(float s)
{
    Transform t;
    t = *this;
    if (t.flags & 2)
        t.p *= s;
    return t;
}
// MATCH: jgld.dll 0x100060d0 ??XTransform@@QAEAAV0@ABV0@@Z
Transform& Transform::operator*=(const Transform& o)
{
    Transform t;
    if (flags == 0 || o.flags == 0) {
        if (o.flags)
            *this = o;
    } else {
        if (o.flags & 0x1c) {
            if (flags & 2)
                p.rotate(&o.q);
            if (flags & 0x1c)
                q *= o.q;
        }
        if (o.flags & 2) {
            if (flags & 2)
                p += o.p;
            else
                p = o.p;
        }
    }
    flags |= o.flags;
    return *this;
}
// MATCH: jgld.dll 0x10006340 ?rotate@Transform@@QAEXMABVVector3@@@Z
void Transform::rotate(float angle, const Vector3& axis)
{
    Quat r;
    if (angle == 0.0f)
        return;
    {
        if (flags == 0)
            q.setAxisAngle(angle, axis);
        else {
            r.setAxisAngle(angle, axis);
            if (flags & 2)
                p.rotate(&r);
            q *= r;
        }
        flags |= 0x1c;
    }
}
// MATCH: jgld.dll 0x10006420 ?rotateX@Transform@@QAEXM@Z
void Transform::rotateX(float angle)
{
    Vector3 axis;
    Quat r;
    if (angle == 0.0f)
        return;
    {
        axis.set(1.0f, 0.0f, 0.0f);
        if (flags == 0)
            q.setAxisAngle(angle, axis);
        else {
            r.setAxisAngle(angle, axis);
            if (flags & 2)
                p.rotate(&r);
            q *= r;
        }
        flags |= 4;
    }
}
// MATCH: jgld.dll 0x10006520 ?rotateY@Transform@@QAEXM@Z
void Transform::rotateY(float angle)
{
    Vector3 axis;
    Quat r;
    if (angle == 0.0f)
        return;
    {
        axis.set(0.0f, 1.0f, 0.0f);
        if (flags == 0)
            q.setAxisAngle(angle, axis);
        else {
            r.setAxisAngle(angle, axis);
            if (flags & 2)
                p.rotate(&r);
            q *= r;
        }
        flags |= 8;
    }
}
// MATCH: jgld.dll 0x10006620 ?rotateZ@Transform@@QAEXM@Z
void Transform::rotateZ(float angle)
{
    Vector3 axis;
    Quat r;
    if (angle == 0.0f)
        return;
    {
        axis.set(0.0f, 0.0f, 1.0f);
        if (flags == 0)
            q.setAxisAngle(angle, axis);
        else {
            r.setAxisAngle(angle, axis);
            if (flags & 2)
                p.rotate(&r);
            q *= r;
        }
        flags |= 0x10;
    }
}

// MATCH: jgld.dll 0x10004f40 ??DMatrix@@QAE?AV0@M@Z
Matrix Matrix::operator*(float s)
{
    int i;
    Matrix r;
    for (i = 0; i < 15; i += 5)
        r.m[i] = s * m[i];
    return r;
}

// MATCH: jgld.dll 0x10005030 ??DMatrix@@QAE?AV0@ABV0@@Z
Matrix Matrix::operator*(const Matrix& o)
{
    Matrix r;
    int i;
    int k = 0;
    for (i = 0; i < 4; i++) {
        r.m[k] = m[k] * o.m[0] + m[k + 1] * o.m[4] + m[k + 2] * o.m[8] + m[k + 3] * o.m[12];
        r.m[k + 1] = m[k] * o.m[1] + m[k + 1] * o.m[5] + m[k + 2] * o.m[9] + m[k + 3] * o.m[13];
        r.m[k + 2] = m[k] * o.m[2] + m[k + 1] * o.m[6] + m[k + 2] * o.m[10] + m[k + 3] * o.m[14];
        r.m[k + 3] = m[k] * o.m[3] + m[k + 1] * o.m[7] + m[k + 2] * o.m[11] + m[k + 3] * o.m[15];
        k += 4;
    }
    return r;
}
