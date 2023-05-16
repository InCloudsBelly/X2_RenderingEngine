#pragma once

template<typename T>
class ChildBrotherTree
{
public:
	struct Iterator
	{
		friend class ChildBrotherTree<T>;
	private:
		ChildBrotherTree<T>* m_node;
		Iterator(ChildBrotherTree<T>* node);
	public:
		Iterator();
		~Iterator();
		inline bool IsValid();
		inline typename T* Node();
		inline typename Iterator operator++();
	};
private:
	T* m_parent;
	T* m_brother;
	T* m_child;
public:
	ChildBrotherTree();
	inline T* Parent();
	inline T* Brother();
	inline T* Child();
	inline void AddChild(T* child);
	inline void AddBrother(T* brother);
	inline typename T* RemoveSelf();
	inline ChildBrotherTree<T>::Iterator ChildIterator();
	inline ChildBrotherTree<T>::Iterator BrotherIterator();
};
template<typename T>
inline ChildBrotherTree<T>::ChildBrotherTree()
	: m_parent(nullptr)
	, m_brother(nullptr)
	, m_child(nullptr)
{
}
template<typename T>
inline T* ChildBrotherTree<T>::Parent()
{
	return m_parent;
}
template<typename T>
inline T* ChildBrotherTree<T>::Brother()
{
	return m_brother;
}
template<typename T>
inline T* ChildBrotherTree<T>::Child()
{
	return m_child;
}
template<typename T>
inline void ChildBrotherTree<T>::AddChild(T* child)
{
	ChildBrotherTree<T>* childNode = static_cast<ChildBrotherTree<T>*>(child);
	childNode->m_parent = static_cast<T*>(this);
	if (m_child)
	{
		ChildBrotherTree<T>* firstChildNode = static_cast<ChildBrotherTree<T>*>(m_child);
		firstChildNode->AddBrother(child);
	}
	else
	{
		m_child = child;
	}
}
template<typename T>
inline void ChildBrotherTree<T>::AddBrother(T* brother)
{
	ChildBrotherTree<T>* brotherNode = static_cast<ChildBrotherTree<T>*>(brother);
	brotherNode->m_parent = m_parent;
	ChildBrotherTree<T>* b = static_cast<ChildBrotherTree<T>*>(m_brother);
	if (b)
	{
		while (b->m_brother)
		{
			b = static_cast<ChildBrotherTree<T>*>(b->m_brother);
		}
		b->m_brother = brother;
	}
	else
	{
		m_brother = brother;
	}
}
template<typename T>
inline typename T* ChildBrotherTree<T>::RemoveSelf()
{
	ChildBrotherTree<T>* result = nullptr;
	if (m_parent)
	{
		ChildBrotherTree<T>* pre = nullptr;
		ChildBrotherTree<T>* o = static_cast<ChildBrotherTree<T>*>(this->m_parent);
		for (Iterator iter = o->ChildIterator(); iter.IsValid(); ++iter)
		{
			if (iter.m_node == this)
			{
				break;
			}
			pre = iter.m_node;
		}
		if (pre)
		{
			pre->m_brother = m_brother;
			m_parent = nullptr;
			m_brother = nullptr;
		}
		else
		{
			static_cast<ChildBrotherTree<T>*>(m_parent)->m_child = m_brother;
		}
		m_parent = nullptr;
		m_brother = nullptr;
		result = this;
	}
	else
	{
		m_parent = nullptr;
		m_brother = nullptr;
		result = this;
	}

	return static_cast<T*>(result);
}
template<typename T>
inline typename ChildBrotherTree<T>::Iterator ChildBrotherTree<T>::ChildIterator()
{
	return ChildBrotherTree<T>::Iterator(this);
}
template<typename T>
inline typename ChildBrotherTree<T>::Iterator ChildBrotherTree<T>::BrotherIterator()
{
	return ChildBrotherTree<T>::Iterator(static_cast<ChildBrotherTree<T>*>(this->m_parent));
}
template<typename T>
inline ChildBrotherTree<T>::Iterator::Iterator(ChildBrotherTree<T>* node)
	: m_node(node)
{
}
template<typename T>
inline ChildBrotherTree<T>::Iterator::Iterator()
	: m_node(nullptr)
{
}
template<typename T>
inline ChildBrotherTree<T>::Iterator::~Iterator()
{
}
template<typename T>
inline bool ChildBrotherTree<T>::Iterator::IsValid()
{
	return m_node;
}
template<typename T>
inline typename T* ChildBrotherTree<T>::Iterator::Node()
{
	return static_cast<T*>(m_node);
}
template<typename T>
inline typename ChildBrotherTree<T>::Iterator ChildBrotherTree<T>::Iterator::operator++()
{
	m_node = static_cast<ChildBrotherTree<T>*>(m_node->m_brother);
	return *this;
}