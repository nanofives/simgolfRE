#ifndef JGLMATH_H
#define JGLMATH_H

#define XF_TRANSLATE    0x02
#define XF_ROTATE_X     0x04
#define XF_ROTATE_Y     0x08
#define XF_ROTATE_Z     0x10
#define XF_ROTATE       (XF_ROTATE_X | XF_ROTATE_Y | XF_ROTATE_Z)

template <class Real> struct Quaternion;

template <class Real>
struct Vector3 {
    Real x, y, z;

    void     Set(Real fx, Real fy, Real fz);
    void     Zero();
    Vector3  operator+(const Vector3 &rhs) const;
    Vector3  operator-(const Vector3 &rhs) const;
    Vector3 &operator+=(const Vector3 &rhs);
    Vector3 &operator-=(const Vector3 &rhs);
    Vector3  operator*(Real s) const;
    Vector3 &operator*=(Real s);
    bool     operator==(const Vector3 &rhs) const;
    Real     Distance(const Vector3 &p) const;
    Real     DistanceSquared(const Vector3 &p) const;
    Real     Dot(const Vector3 &v) const;
    void     Normalize();
    void     Negate();
    Vector3  Cross(const Vector3 &v) const;
    void     ScaleBy(Real s);
    Vector3  Midpoint(const Vector3 &v);
    void     Add(const Vector3 &v);
    void     Add(Real dx, Real dy, Real dz);
    void     Rotate(const Quaternion<Real> *q);
};

template <class Real>
struct Quaternion {
    Vector3<Real> v;
    Real          w;

    Quaternion()
    {
        v.Set(0.0f, 0.0f, 0.0f);
        w = 1.0f;
    }
    Quaternion *Assign(const Quaternion *q)
    {
        if (q)
            *this = *q;
        return this;
    }
    Quaternion(Real degrees, const Vector3<Real> &axis)
    {
        SetAxisAngle(degrees, axis);
    }
    void Identity()
    {
        v.Set(0.0f, 0.0f, 0.0f);
        w = 1.0f;
    }
    void        SetAxisAngle(Real degrees, const Vector3<Real> &axis);
    Quaternion &operator*=(const Quaternion &r);
    Quaternion  operator*(const Quaternion &r) const;
    void        Conjugate();
};

template <class Real>
struct Matrix4 {
    Matrix4();
    Matrix4(const Quaternion<Real> *q, const Vector3<Real> *t)
    {
        FromQuaternionTranslation(q, t);
    }
    Matrix4(const Quaternion<Real> &q)
    {
        FromQuaternion(q);
    }
    Matrix4(const Vector3<Real> *t)
    {
        SetTranslation(t);
    }
    virtual ~Matrix4()
    {
        Identity();
    }
    void Reset()
    {
        Identity();
    }
    void          Identity();
    void          FromQuaternionTranslation(const Quaternion<Real> *q, const Vector3<Real> *t);
    void          SetTranslation(const Vector3<Real> *t);
    void          FromQuaternion(const Quaternion<Real> &q);
    Matrix4       Add(const Matrix4 *rhs);
    Matrix4      *ScaleInto(Real s);
    Matrix4      *MultiplyInto(const Matrix4 *rhs);
    Matrix4      *AddInto(const Matrix4 *rhs);
    Matrix4      *SubInto(const Matrix4 *rhs);
    bool          Equals(const Matrix4 *rhs) const;
    Vector3<Real> TransformPoint(const Vector3<Real> &v) const;
    Matrix4       ScaleDiagonal(Real s);
    Matrix4       Multiply(const Matrix4 *rhs);
    Matrix4      *MakeIdentity();
    Matrix4      *Transpose();
    Matrix4      *Invert();
    Matrix4      *Translate(Real dx, Real dy, Real dz);
    Matrix4      *Translate(const Vector3<Real> &v);

    union {
        Real f[16];
        Real m[4][4];
    };
};

template <class Real>
struct Transform {
    Quaternion<Real> m_rot;
    Vector3<Real>    m_pos;
    unsigned int     m_flags;

    Transform();
    Transform(const Quaternion<Real> *q, const Vector3<Real> *p)
    {
        SetRotationTranslation(q, p);
    }
    Transform(const Quaternion<Real> *q)
    {
        SetRotation(q);
    }
    Transform(const Vector3<Real> *p)
    {
        SetTranslation(p);
    }
    virtual ~Transform()
    {
        Reset();
    }
    void Set(const Quaternion<Real> *q, const Vector3<Real> *p)
    {
        m_rot = *q;
        m_pos = *p;
    }
    void Identity()
    {
        Reset();
    }
    void          SetTranslation(const Vector3<Real> *p);
    void          SetRotation(const Quaternion<Real> *q);
    void          SetRotationTranslation(const Quaternion<Real> *q, const Vector3<Real> *p);
    void          Reset();
    void          BuildMatrix(Matrix4<Real> &mat) const;
    Transform     operator+(const Vector3<Real> &delta) const;
    Transform    &operator+=(const Vector3<Real> &delta);
    Transform     operator-(const Vector3<Real> &delta) const;
    Transform    &operator-=(const Vector3<Real> &delta);
    Transform    &Translate(Real x, Real y, Real z);
    Transform    &Offset(const Vector3<Real> &delta);
    Vector3<Real> TransformPoint(const Vector3<Real> &v) const;
    Transform     operator*(const Transform &o) const;
    Transform     Scale(Real s) const;
    Transform    &operator*=(const Transform &o);
    Transform    &ScaleTranslation(Real s);
    void          Invert();
    void          Rotate(Real degrees, const Vector3<Real> &axis);
    void          RotateX(Real degrees);
    void          RotateY(Real degrees);
    void          RotateZ(Real degrees);
};

typedef Vector3<float>    Vector3f;
typedef Quaternion<float> Quaternionf;
typedef Matrix4<float>    Matrix4f;
typedef Transform<float>  Transformf;
#endif
