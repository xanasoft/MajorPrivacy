#pragma once
#include "../lib_global.h"

class CSharedData
{
public:
    mutable QAtomicInt ref;

    inline CSharedData() noexcept : ref(0) { }
    inline CSharedData(const CSharedData &) noexcept : ref(0) { }

    // using the assignment operator would lead to corruption in the ref-counting
    CSharedData &operator=(const CSharedData &) = delete;
    virtual ~CSharedData() = default;

    virtual CSharedData* Clone() = 0;
};

template <class T> class CSharedDataPtr
{
public:
    typedef T Type;
    typedef T *pointer;

    inline void detach() { if (d && d->ref.loadRelaxed() != 1) detach_helper(); }
    inline T &operator*() { detach(); return *d; }
    inline const T &operator*() const { return *d; }
    inline T *operator->() { detach(); return d; }
    inline const T *operator->() const { return d; }
    inline operator T *() { detach(); return d; }
    inline operator const T *() const { return d; }
    inline T *data() { detach(); return d; }
    inline const T *data() const { return d; }
    inline const T *constData() const { return d; }

    inline bool operator==(const CSharedDataPtr<T> &other) const { return d == other.d; }
    inline bool operator!=(const CSharedDataPtr<T> &other) const { return d != other.d; }

    inline CSharedDataPtr() { d = nullptr; }
    inline ~CSharedDataPtr() { if (d && !d->ref.deref()) delete d; }

    explicit CSharedDataPtr(T *data) noexcept;
    inline CSharedDataPtr(const CSharedDataPtr<T> &o) : d(o.d) { if (d) d->ref.ref(); }
    inline CSharedDataPtr<T> & operator=(const CSharedDataPtr<T> &o) {
        if (o.d != d) {
            if (o.d)
                o.d->ref.ref();
            T *old = d;
            d = o.d;
            if (old && !old->ref.deref())
                delete old;
        }
        return *this;
    }
    inline CSharedDataPtr &operator=(T *o) {
        if (o != d) {
            if (o)
                o->ref.ref();
            T *old = d;
            d = o;
            if (old && !old->ref.deref())
                delete old;
        }
        return *this;
    }
    CSharedDataPtr(CSharedDataPtr &&o) noexcept : d(o.d) { o.d = nullptr; }
    inline CSharedDataPtr<T> &operator=(CSharedDataPtr<T> &&other) noexcept
    {
        CSharedDataPtr moved(std::move(other));
        swap(moved);
        return *this;
    }

    inline bool operator!() const { return !d; }

    inline void swap(CSharedDataPtr &other) noexcept
    { std::swap(d, other.d); }

protected:
    T *clone();

private:
    void detach_helper();

    T *d;
};

template <class T> inline bool operator==(std::nullptr_t p1, const CSharedDataPtr<T> &p2)
{
    Q_UNUSED(p1);
    return !p2;
}

template <class T> inline bool operator==(const CSharedDataPtr<T> &p1, std::nullptr_t p2)
{
    Q_UNUSED(p2);
    return !p1;
}

template <class T> inline CSharedDataPtr<T>::CSharedDataPtr(T *adata) noexcept
    : d(adata)
{ if (d) d->ref.ref(); }

template <class T> inline T *CSharedDataPtr<T>::clone()
{
    return (T*)d->Clone();
}

template <class T> inline void CSharedDataPtr<T>::detach_helper()
{
    T *x = clone();
    x->ref.ref();
    if (!d->ref.deref())
        delete d;
    d = x;
}