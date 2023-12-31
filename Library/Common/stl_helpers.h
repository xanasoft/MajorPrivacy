#pragma once
#include <memory>

template <class T>
class CCowPtr
{
public:
	CCowPtr(T* ptr = NULL)							{ m_ptr = std::shared_ptr<T>(ptr);}
	CCowPtr(const CCowPtr<T>& Ptr)					{ m_ptr = Ptr.m_ptr; }

	CCowPtr<T>& operator=(T* ptr)					{ m_ptr = std::shared_ptr<T>(ptr); return *this; }
	CCowPtr<T>& operator=(const CCowPtr<T>& Ptr)	{ m_ptr = Ptr.m_ptr; return *this; }
	
	template <class... _Types>
	void New(_Types&&... _Args)						{ m_ptr = std::make_shared<T>(_STD forward<_Types>(_Args)...); }
	void Delete()									{ m_ptr.reset(); }

    inline const T* operator->() const				{ return m_ptr; }
	inline T* operator->()							{ DetachIfShared(); return m_ptr.get(); }
	inline const T& operator*() const				{ return *m_ptr; }
	inline T& operator*()							{ DetachIfShared(); return *m_ptr; }
	inline const T& Const() const					{ return *m_ptr; }
	inline T* Ptr()									{ DetachIfShared(); return m_ptr; }

private:

	void DetachIfShared()
	{
		if (m_ptr.use_count() > 1) {
			std::shared_ptr<T> ptr = m_ptr;
			m_ptr = std::shared_ptr<T>(new T(*ptr));
		}
	}

	std::shared_ptr<T> m_ptr;
};

template <class T, typename K, typename V>
void mmap_erase(T& mmap, const K& key, const V& value) 
{
	for (auto I = mmap.find(key); I != mmap.end() && I->first == key; ++I) {
        if (I->second == value) {
            mmap.erase(I);
            break;
        }
    }
}