#include "jgldefs.h"
#include "jglmath.h"
#include <math.h>

class JGLSys;

extern "C" {
JGLSys *graphsys;
}

template <class Real>
void Vector3<Real>::Set(Real fx, Real fy, Real fz)
{
    x = fx;
    y = fy;
    z = fz;
}

template <class Real>
void Vector3<Real>::Zero()
{
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
}

template <class Real>
Vector3<Real> Vector3<Real>::operator+(const Vector3<Real> &rhs) const
{
    Vector3<Real> t;
    int           i;
    for (i = 0; i < 3; i += 1)
        (&t.x)[i] = (&x)[i] + (&rhs.x)[i];
    return t;
}

template <class Real>
Vector3<Real> Vector3<Real>::operator-(const Vector3<Real> &rhs) const
{
    Vector3<Real> r;
    for (int i = 0; i < 3; i++)
        (&r.x)[i] = (&x)[i] - (&rhs.x)[i];
    return r;
}

template <class Real>
Vector3<Real> &Vector3<Real>::operator+=(const Vector3<Real> &rhs)
{
    for (int i = 0; i < 3; i++)
        (&x)[i] = (&x)[i] + (&rhs.x)[i];
    return *this;
}

template <class Real>
Vector3<Real> &Vector3<Real>::operator-=(const Vector3<Real> &rhs)
{
    for (int i = 0; i < 3; i++)
        (&x)[i] = (&x)[i] - (&rhs.x)[i];
    return *this;
}

template <class Real>
Vector3<Real> Vector3<Real>::operator*(Real s) const
{
    Vector3<Real> t;
    int           i;
    for (i = 0; i < 3; i += 1)
        (&t.x)[i] = s * (&x)[i];
    return t;
}

template <class Real>
Vector3<Real> &Vector3<Real>::operator*=(Real s)
{
    for (int i = 0; i < 3; i++)
        (&x)[i] = s * (&x)[i];
    return *this;
}

template <class Real>
bool Vector3<Real>::operator==(const Vector3<Real> &rhs) const
{
    if (x == rhs.x && y == rhs.y && z == rhs.z)
        return true;
    return false;
}

template <class Real>
Real Vector3<Real>::Distance(const Vector3<Real> &p) const
{
    double sum;
    int    i;
    sum = 0.0;
    for (i = 0; i < 3; i++)
        sum += ((&x)[i] - (&p.x)[i]) * ((&x)[i] - (&p.x)[i]);
    return (Real)(sum = sqrt(sum));
}

template <class Real>
Real Vector3<Real>::DistanceSquared(const Vector3<Real> &p) const
{
    Real sum = 0.0f;
    for (int i = 0; i < 3; i++)
        sum = ((&x)[i] - (&p.x)[i]) * ((&x)[i] - (&p.x)[i]) + sum;
    return sum;
}

template <class Real>
Real Vector3<Real>::Dot(const Vector3<Real> &v) const
{
    return x * v.x + y * v.y + z * v.z;
}

template <class Real>
void Vector3<Real>::Normalize()
{
    double len;
    len = x * x + y * y + z * z;
    len = sqrt(len);
    if (len == 0.0 || len == 1.0)
        return;
    len = 1.0 / len;
    x *= len;
    y *= len;
    z *= len;
}

template <class Real>
void Vector3<Real>::Negate()
{
    x = -x;
    y = -y;
    z = -z;
}

template <class Real>
Vector3<Real> Vector3<Real>::Cross(const Vector3<Real> &v) const
{
    Vector3<Real> r;
    r.x = y * v.z - z * v.y;
    r.y = -x * v.z + z * v.x;
    r.z = x * v.y - y * v.x;
    return r;
}

template <class Real>
void Vector3<Real>::ScaleBy(Real s)
{
    for (int i = 0; i < 3; i++)
        (&x)[i] = s * (&x)[i];
}

template <class Real>
Vector3<Real> Vector3<Real>::Midpoint(const Vector3<Real> &v)
{
    Vector3<Real> r;
    int           i;
    for (i = 0; i < 3; i++)
        (&r.x)[i] = ((&v.x)[i] + (&x)[i]) * 0.5f * (&r.x)[i];
    return r;
}

template <class Real>
void Vector3<Real>::Add(const Vector3<Real> &v)
{
    x += v.x;
    y += v.y;
    z += v.z;
}

template <class Real>
void Vector3<Real>::Add(Real dx, Real dy, Real dz)
{
    x += dx;
    y += dy;
    z += dz;
}

template <class Real>
void Vector3<Real>::Rotate(const Quaternion<Real> *q)
{
    Quaternion<Real> t;
    Quaternion<Real> c;
    if (q == 0)
        return;
    c = *q;
    t.w = 0.0f;
    t.v = *this;
    t = c * t;
    c.Conjugate();
    t = t * c;
    *this = t.v;
}

template <class Real>
void Quaternion<Real>::SetAxisAngle(Real degrees, const Vector3<Real> &axis)
{
    Real halfAngle;
    Real c;
    Real s;
    halfAngle = 0.5f * degrees * g_fDegToRad;
    c = (Real)cos(halfAngle);
    s = (Real)sin(halfAngle);
    w = c;
    v = axis;
    v *= s;
}

template <class Real>
Quaternion<Real> &Quaternion<Real>::operator*=(const Quaternion<Real> &r)
{
    Quaternion<Real> q;
    q.w = w * r.w - v.Dot(r.v);
    q.v = v.Cross(r.v) + r.v * w + v * r.w;
    *this = q;
    return *this;
}

template <class Real>
Quaternion<Real> Quaternion<Real>::operator*(const Quaternion<Real> &r) const
{
    Quaternion<Real> q;
    q.w = w * r.w - v.Dot(r.v);
    q.v = v.Cross(r.v) + r.v * w + v * r.w;
    return q;
}

template <class Real>
void Quaternion<Real>::Conjugate()
{
    v.x = -v.x;
    v.y = -v.y;
    v.z = -v.z;
}

template <class Real>
Matrix4<Real>::Matrix4()
{
    Identity();
}

template <class Real>
void Matrix4<Real>::Identity()
{
    for (int i = 0; i < 16; i++)
        f[i] = 0.0f;
    f[0] = 1.0f;
    f[5] = 1.0f;
    f[10] = 1.0f;
    f[15] = 1.0f;
}

template <class Real>
void Matrix4<Real>::FromQuaternionTranslation(const Quaternion<Real> *q, const Vector3<Real> *t)
{
    FromQuaternion(*q);
    SetTranslation(t);
}

template <class Real>
void Matrix4<Real>::SetTranslation(const Vector3<Real> *t)
{
    f[12] = t->x;
    f[13] = t->y;
    f[14] = t->z;
}

template <class Real>
void Matrix4<Real>::FromQuaternion(const Quaternion<Real> &q)
{
    Real wx, wy, wz, xx, xy, xz, yy, yz, zz, x2, y2, z2;
    x2 = q.v.x + q.v.x;
    y2 = q.v.y + q.v.y;
    z2 = q.v.z + q.v.z;
    xx = q.v.x * x2;
    xy = q.v.x * y2;
    xz = q.v.x * z2;
    yy = q.v.y * y2;
    yz = q.v.y * z2;
    zz = q.v.z * z2;
    wx = q.w * x2;
    wy = q.w * y2;
    wz = q.w * z2;
    f[0] = 1.0f - (yy + zz);
    f[4] = xy - wz;
    f[8] = xz + wy;
    f[1] = xy + wz;
    f[5] = 1.0f - (xx + zz);
    f[9] = yz - wx;
    f[2] = xz - wy;
    f[6] = yz + wx;
    f[10] = 1.0f - (xx + yy);
}

template <class Real>
Matrix4<Real> Matrix4<Real>::Add(const Matrix4<Real> *rhs)
{
    Matrix4<Real> ret;
    int           i;
    for (i = 0; i < 16; i++)
        ret.f[i] = f[i] + rhs->f[i];
    return ret;
}

template <class Real>
Matrix4<Real> *Matrix4<Real>::ScaleInto(Real s)
{
    for (int i = 0; i < 15; i += 5)
        f[i] = s * f[i];
    return this;
}

template <class Real>
Matrix4<Real> *Matrix4<Real>::MultiplyInto(const Matrix4<Real> *rhs)
{
    Matrix4<Real> ret;
    int           i;
    int           row = 0;
    for (i = 0; i < 4; i++) {
        ret.f[row] = f[row] * rhs->f[0] + f[row + 1] * rhs->f[4] + f[row + 2] * rhs->f[8] + f[row + 3] * rhs->f[12];
        ret.f[row + 1] = f[row] * rhs->f[1] + f[row + 1] * rhs->f[5] + f[row + 2] * rhs->f[9] + f[row + 3] * rhs->f[13];
        ret.f[row + 2] =
            f[row] * rhs->f[2] + f[row + 1] * rhs->f[6] + f[row + 2] * rhs->f[10] + f[row + 3] * rhs->f[14];
        ret.f[row + 3] =
            f[row] * rhs->f[3] + f[row + 1] * rhs->f[7] + f[row + 2] * rhs->f[11] + f[row + 3] * rhs->f[15];
        row += 4;
    }
    *this = ret;
    return this;
}

template <class Real>
Matrix4<Real> *Matrix4<Real>::AddInto(const Matrix4<Real> *rhs)
{
    for (int i = 0; i < 16; i++)
        f[i] = f[i] + rhs->f[i];
    return this;
}

template <class Real>
Matrix4<Real> *Matrix4<Real>::SubInto(const Matrix4<Real> *rhs)
{
    for (int i = 0; i < 16; i++)
        f[i] = f[i] - rhs->f[i];
    return this;
}

template <class Real>
bool Matrix4<Real>::Equals(const Matrix4<Real> *rhs) const
{
    for (int i = 0; i < 16; i++)
        if (f[i] != rhs->f[i])
            return false;
    return true;
}

template <class Real>
Vector3<Real> Matrix4<Real>::TransformPoint(const Vector3<Real> &v) const
{
    Vector3<Real> r;
    r.x = f[0] * v.x + f[4] * v.y + f[8] * v.z + f[12];
    r.y = f[1] * v.x + f[5] * v.y + f[9] * v.z + f[13];
    r.z = f[2] * v.x + f[6] * v.y + f[10] * v.z + f[14];
    return r;
}

template <class Real>
Matrix4<Real> Matrix4<Real>::ScaleDiagonal(Real s)
{
    int           i;
    Matrix4<Real> ret;
    for (i = 0; i < 15; i += 5)
        ret.f[i] = s * f[i];
    return ret;
}

template <class Real>
Matrix4<Real> Matrix4<Real>::Multiply(const Matrix4<Real> *rhs)
{
    Matrix4<Real> ret;
    int           i;
    int           row = 0;
    for (i = 0; i < 4; i++) {
        ret.f[row] = f[row] * rhs->f[0] + f[row + 1] * rhs->f[4] + f[row + 2] * rhs->f[8] + f[row + 3] * rhs->f[12];
        ret.f[row + 1] = f[row] * rhs->f[1] + f[row + 1] * rhs->f[5] + f[row + 2] * rhs->f[9] + f[row + 3] * rhs->f[13];
        ret.f[row + 2] =
            f[row] * rhs->f[2] + f[row + 1] * rhs->f[6] + f[row + 2] * rhs->f[10] + f[row + 3] * rhs->f[14];
        ret.f[row + 3] =
            f[row] * rhs->f[3] + f[row + 1] * rhs->f[7] + f[row + 2] * rhs->f[11] + f[row + 3] * rhs->f[15];
        row += 4;
    }
    return ret;
}

template <class Real>
Matrix4<Real> *Matrix4<Real>::MakeIdentity()
{
    Identity();
    return this;
}

template <class Real>
Matrix4<Real> *Matrix4<Real>::Transpose()
{
    return this;
}

template <class Real>
Matrix4<Real> *Matrix4<Real>::Invert()
{
    return this;
}

template <class Real>
Matrix4<Real> *Matrix4<Real>::Translate(Real dx, Real dy, Real dz)
{
    f[12] += dx;
    f[13] += dy;
    f[14] += dz;
    return this;
}

template <class Real>
Matrix4<Real> *Matrix4<Real>::Translate(const Vector3<Real> &v)
{
    f[12] += v.x;
    f[13] += v.y;
    f[14] += v.z;
    return this;
}

template <class Real>
Transform<Real>::Transform()
{
    Reset();
}

template <class Real>
void Transform<Real>::SetTranslation(const Vector3<Real> *p)
{
    m_flags = XF_TRANSLATE;
    m_rot.Identity();
    m_pos = *p;
}

template <class Real>
void Transform<Real>::SetRotation(const Quaternion<Real> *q)
{
    m_flags = XF_ROTATE;
    m_rot = *q;
    m_pos.Set(0.0f, 0.0f, 0.0f);
}

template <class Real>
void Transform<Real>::SetRotationTranslation(const Quaternion<Real> *q, const Vector3<Real> *p)
{
    m_flags = XF_ROTATE | XF_TRANSLATE;
    m_rot = *q;
    m_pos = *p;
}

template <class Real>
void Transform<Real>::Reset()
{
    m_flags = 0;
}

template <class Real>
void Transform<Real>::BuildMatrix(Matrix4<Real> &mat) const
{
    mat.MakeIdentity();
    if (m_flags & XF_ROTATE) {
        Real wx, wy, wz, xx, xy, xz, yy, yz, zz, x2, y2, z2;
        x2 = m_rot.v.x + m_rot.v.x;
        y2 = m_rot.v.y + m_rot.v.y;
        z2 = m_rot.v.z + m_rot.v.z;
        xx = x2 * m_rot.v.x;
        xy = y2 * m_rot.v.x;
        xz = z2 * m_rot.v.x;
        yy = y2 * m_rot.v.y;
        yz = z2 * m_rot.v.y;
        zz = z2 * m_rot.v.z;
        wx = x2 * m_rot.w;
        wy = y2 * m_rot.w;
        wz = z2 * m_rot.w;
        mat.f[0] = 1.0f - (yy + zz);
        mat.f[4] = xy - wz;
        mat.f[8] = xz + wy;
        mat.f[1] = xy + wz;
        mat.f[5] = 1.0f - (xx + zz);
        mat.f[9] = yz - wx;
        mat.f[2] = xz - wy;
        mat.f[6] = yz + wx;
        mat.f[10] = 1.0f - (xx + yy);
    }
    if (m_flags & XF_TRANSLATE) {
        mat.f[12] = m_pos.x;
        mat.f[13] = m_pos.y;
        mat.f[14] = m_pos.z;
    }
}

template <class Real>
Transform<Real> Transform<Real>::operator+(const Vector3<Real> &delta) const
{
    Transform<Real> ret;
    ret = *this;
    if (ret.m_flags & XF_TRANSLATE) {
        ret.m_pos += delta;
    } else {
        ret.m_flags = ret.m_flags | XF_TRANSLATE;
        ret.m_pos = delta;
    }
    return ret;
}

template <class Real>
Transform<Real> &Transform<Real>::operator+=(const Vector3<Real> &delta)
{
    if (m_flags & XF_TRANSLATE) {
        m_pos += delta;
    } else {
        m_flags = m_flags | XF_TRANSLATE;
        m_pos = delta;
    }
    return *this;
}

template <class Real>
Transform<Real> Transform<Real>::operator-(const Vector3<Real> &delta) const
{
    Transform<Real> ret;
    ret = *this;
    if (ret.m_flags & XF_TRANSLATE) {
        ret.m_pos -= delta;
    } else {
        ret.m_flags = ret.m_flags | XF_TRANSLATE;
        ret.m_pos = delta;
    }
    return ret;
}

template <class Real>
Transform<Real> &Transform<Real>::operator-=(const Vector3<Real> &delta)
{
    if (m_flags & XF_TRANSLATE) {
        m_pos -= delta;
    } else {
        m_flags = m_flags | XF_TRANSLATE;
        m_pos = delta;
    }
    return *this;
}

template <class Real>
Transform<Real> &Transform<Real>::Translate(Real x, Real y, Real z)
{
    if (m_flags & XF_TRANSLATE) {
        m_pos.Add(x, y, z);
    } else {
        m_flags = m_flags | XF_TRANSLATE;
        m_pos.Set(x, y, z);
    }
    return *this;
}

template <class Real>
Transform<Real> &Transform<Real>::Offset(const Vector3<Real> &delta)
{
    if (m_flags & XF_TRANSLATE) {
        m_pos += delta;
    } else {
        m_flags = m_flags | XF_TRANSLATE;
        m_pos = delta;
    }
    return *this;
}

template <class Real>
Vector3<Real> Transform<Real>::TransformPoint(const Vector3<Real> &v) const
{
    Vector3<Real> r = v;
    if (m_flags & XF_ROTATE)
        r.Rotate(&m_rot);
    if (m_flags & XF_TRANSLATE)
        r += m_pos;
    return r;
}

template <class Real>
Transform<Real> Transform<Real>::operator*(const Transform<Real> &o) const
{
    Transform<Real> tmp;
    if (m_flags == 0 || o.m_flags == 0) {
        if (o.m_flags != 0)
            tmp = o;
        else if (m_flags == 0)
            tmp = *this;
    } else {
        tmp = *this;
        if (o.m_flags & XF_ROTATE) {
            if (m_flags & XF_TRANSLATE)
                tmp.m_pos.Rotate(&o.m_rot);
            if (m_flags & XF_ROTATE)
                tmp.m_rot = m_rot * o.m_rot;
            else
                tmp.m_rot = m_rot;
        }
        if (o.m_flags & XF_TRANSLATE) {
            if (tmp.m_flags & XF_TRANSLATE)
                tmp.m_pos += o.m_pos;
            else
                tmp.m_pos = o.m_pos;
        }
    }
    tmp.m_flags = m_flags | o.m_flags;
    return tmp;
}

template <class Real>
Transform<Real> Transform<Real>::Scale(Real s) const
{
    Transform<Real> ret;
    ret = *this;
    if (ret.m_flags & XF_TRANSLATE)
        ret.m_pos.ScaleBy(s);
    return ret;
}

template <class Real>
Transform<Real> &Transform<Real>::operator*=(const Transform<Real> &o)
{
    Transform<Real> tmp;
    if (m_flags == 0 || o.m_flags == 0) {
        if (o.m_flags != 0)
            *this = o;
    } else {
        if (o.m_flags & XF_ROTATE) {
            if (m_flags & XF_TRANSLATE)
                m_pos.Rotate(&o.m_rot);
            if (m_flags & XF_ROTATE)
                m_rot *= o.m_rot;
        }
        if (o.m_flags & XF_TRANSLATE) {
            if (m_flags & XF_TRANSLATE)
                m_pos += o.m_pos;
            else
                m_pos = o.m_pos;
        }
    }
    m_flags |= o.m_flags;
    return *this;
}

template <class Real>
Transform<Real> &Transform<Real>::ScaleTranslation(Real s)
{
    if (m_flags & XF_TRANSLATE)
        m_pos.ScaleBy(s);
    return *this;
}

template <class Real>
void Transform<Real>::Invert()
{
    m_pos.Negate();
    m_rot.Conjugate();
}

template <class Real>
void Transform<Real>::Rotate(Real degrees, const Vector3<Real> &axis)
{
    Quaternion<Real> q;
    if (degrees == 0.0f)
        return;
    if (m_flags == 0) {
        m_rot.SetAxisAngle(degrees, axis);
    } else {
        q.SetAxisAngle(degrees, axis);
        if (m_flags & XF_TRANSLATE)
            m_pos.Rotate(&q);
        m_rot *= q;
    }
    m_flags |= XF_ROTATE;
}

template <class Real>
void Transform<Real>::RotateX(Real degrees)
{
    Vector3<Real>    axis;
    Quaternion<Real> q;
    if (degrees == 0.0f)
        return;
    axis.Set(1.0f, 0.0f, 0.0f);
    if (m_flags == 0) {
        m_rot.SetAxisAngle(degrees, axis);
    } else {
        q.SetAxisAngle(degrees, axis);
        if (m_flags & XF_TRANSLATE)
            m_pos.Rotate(&q);
        m_rot *= q;
    }
    m_flags |= XF_ROTATE_X;
}

template <class Real>
void Transform<Real>::RotateY(Real degrees)
{
    Vector3<Real>    axis;
    Quaternion<Real> q;
    if (degrees == 0.0f)
        return;
    axis.Set(0.0f, 1.0f, 0.0f);
    if (m_flags == 0) {
        m_rot.SetAxisAngle(degrees, axis);
    } else {
        q.SetAxisAngle(degrees, axis);
        if (m_flags & XF_TRANSLATE)
            m_pos.Rotate(&q);
        m_rot *= q;
    }
    m_flags |= XF_ROTATE_Y;
}

template <class Real>
void Transform<Real>::RotateZ(Real degrees)
{
    Vector3<Real>    axis;
    Quaternion<Real> q;
    if (degrees == 0.0f)
        return;
    axis.Set(0.0f, 0.0f, 1.0f);
    if (m_flags == 0) {
        m_rot.SetAxisAngle(degrees, axis);
    } else {
        q.SetAxisAngle(degrees, axis);
        if (m_flags & XF_TRANSLATE)
            m_pos.Rotate(&q);
        m_rot *= q;
    }
    m_flags |= XF_ROTATE_Z;
}

template struct Vector3<float>;
template struct Quaternion<float>;
template struct Matrix4<float>;
template struct Transform<float>;
