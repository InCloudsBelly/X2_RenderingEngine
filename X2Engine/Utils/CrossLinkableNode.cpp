#include "CrossLinkableNode.h"
//#include "Utils/Log.h"
#include <stdexcept>

CrossLinkableNode::CrossLinkableNode()
	: m_left(nullptr)
	, m_right(nullptr)
	, m_top(nullptr)
	, m_bottom(nullptr)
	, m_colHead(nullptr)
	, m_rowHead(nullptr)
{
}

CrossLinkableNode::~CrossLinkableNode()
{
}

CrossLinkableColHead* CrossLinkableNode::ColHead()
{
	return m_colHead;
}

CrossLinkableRowHead* CrossLinkableNode::RowHead()
{
	return m_rowHead;
}

void CrossLinkableNode::RemoveSelf()
{
	m_colHead->Remove(this);
	m_rowHead->Remove(this);
}

CrossLinkableColHead::CrossLinkableColHead()
	: m_head()
	, m_end(nullptr)
{
	m_head.m_colHead = this;
	m_end = &m_head;
}

CrossLinkableColHead::~CrossLinkableColHead()
{
}

void CrossLinkableColHead::Add(CrossLinkableNode* node)
{
	m_end->m_bottom = node;
	node->m_top = m_end;
	node->m_bottom = nullptr;

	m_end = node;

	node->m_colHead = this;
}

void CrossLinkableColHead::Remove(CrossLinkableNode* node)
{
	if (node->m_colHead != this)
	{
		throw(std::runtime_error("The cross linked node do not blong to this head."));
	}

	if (node != m_end)
	{
		node->m_top->m_bottom = node->m_bottom;
		node->m_bottom->m_top = node->m_top;
	}
	else
	{
		node->m_top->m_bottom = nullptr;

		m_end = node->m_top;
	}

	node->m_top = nullptr;
	node->m_bottom = nullptr;
	node->m_colHead = nullptr;
}

CrossLinkableColHead::Iterator CrossLinkableColHead::Remove(Iterator iterator)
{
	if (!iterator.IsValid())
	{
		throw(std::runtime_error("This Iterator is not valid."));
	}
	auto node = iterator.m_node;
	Iterator next = Iterator();
	if (node->m_colHead != this)
	{
		throw(std::runtime_error("The cross linked node do not blong to this head."));
	}

	if (node != m_end)
	{
		next.m_node = node->m_bottom;

		node->m_top->m_bottom = node->m_bottom;
		node->m_bottom->m_top = node->m_top;
	}
	else
	{
		next.m_node = &node->m_colHead->m_head;

		node->m_top->m_bottom = nullptr;

		m_end = node->m_top;
	}

	node->m_top = nullptr;
	node->m_bottom = nullptr;
	node->m_colHead = nullptr;
	return next;
}

bool CrossLinkableColHead::HaveNode()
{
	return m_end != &m_head;
}

CrossLinkableColHead::Iterator CrossLinkableColHead::GetIterator()
{
	return Iterator(m_head.m_bottom);
}

CrossLinkableColHead::Iterator::Iterator()
	: Iterator(nullptr)
{
}

CrossLinkableColHead::Iterator::Iterator(CrossLinkableNode* node)
	: m_node(node)
{
}

CrossLinkableColHead::Iterator::~Iterator()
{
}

bool CrossLinkableColHead::Iterator::IsValid()
{
	return m_node && m_node != &m_node->m_colHead->m_head;
}

CrossLinkableColHead::Iterator CrossLinkableColHead::Iterator::operator++()
{
	m_node = m_node->m_bottom;
	return *this;
}

CrossLinkableColHead::Iterator CrossLinkableColHead::Iterator::operator++(int)
{
	CrossLinkableColHead::Iterator t = *this;
	m_node = m_node->m_bottom;
	return t;
}

CrossLinkableColHead::Iterator CrossLinkableColHead::Iterator::operator--()
{
	m_node = m_node->m_top;
	return *this;
}

CrossLinkableColHead::Iterator CrossLinkableColHead::Iterator::operator--(int)
{
	CrossLinkableColHead::Iterator t = *this;
	m_node = m_node->m_top;
	return t;
}

CrossLinkableNode* CrossLinkableColHead::Iterator::Node()
{
	return m_node;
}

CrossLinkableRowHead::CrossLinkableRowHead()
	: m_head()
	, m_end(nullptr)
{
	m_head.m_rowHead = this;
	m_end = &m_head;
}

CrossLinkableRowHead::~CrossLinkableRowHead()
{
}

bool CrossLinkableRowHead::HaveNode()
{
	return m_end != &m_head;
}

void CrossLinkableRowHead::Add(CrossLinkableNode* node)
{
	m_end->m_right = node;
	node->m_left = m_end;
	node->m_right = nullptr;

	m_end = node;

	node->m_rowHead = this;
}

void CrossLinkableRowHead::Remove(CrossLinkableNode* node)
{
	if (node->m_rowHead != this)
	{
		throw(std::runtime_error("The cross linked node do not blong to this head."));
	}

	if (node != m_end)
	{
		node->m_left->m_right = node->m_right;
		node->m_right->m_left = node->m_left;
	}
	else
	{
		node->m_left->m_right = nullptr;

		m_end = node->m_left;
	}

	node->m_left = nullptr;
	node->m_right = nullptr;
	node->m_rowHead = nullptr;
}

CrossLinkableRowHead::Iterator CrossLinkableRowHead::Remove(Iterator iterator)
{
	if (!iterator.IsValid())
	{
		throw(std::runtime_error("This Iterator is not valid."));

	}
	auto node = iterator.m_node;
	Iterator next = Iterator();
	if (node->m_rowHead != this)
	{
		throw(std::runtime_error("The cross linked node do not blong to this head."));
	}

	if (node != m_end)
	{
		next.m_node = node->m_right;

		node->m_left->m_right = node->m_right;
		node->m_right->m_left = node->m_left;
	}
	else
	{
		next.m_node = &node->m_rowHead->m_head;

		node->m_left->m_right = nullptr;

		m_end = node->m_left;
	}

	node->m_left = nullptr;
	node->m_right = nullptr;
	node->m_rowHead = nullptr;
	return next;
}

CrossLinkableRowHead::Iterator CrossLinkableRowHead::GetIterator()
{
	return Iterator(m_head.m_right);
}

CrossLinkableRowHead::Iterator::Iterator()
	: Iterator(nullptr)
{
}

CrossLinkableRowHead::Iterator::Iterator(CrossLinkableNode* node)
	: m_node(node)
{
}

CrossLinkableRowHead::Iterator::~Iterator()
{
}

bool CrossLinkableRowHead::Iterator::IsValid()
{
	return m_node && m_node != &m_node->m_rowHead->m_head;
}

CrossLinkableRowHead::Iterator CrossLinkableRowHead::Iterator::operator++()
{
	m_node = m_node->m_right;
	return *this;
}

CrossLinkableRowHead::Iterator CrossLinkableRowHead::Iterator::operator++(int)
{
	CrossLinkableRowHead::Iterator t = *this;
	m_node = m_node->m_right;
	return t;
}

CrossLinkableRowHead::Iterator CrossLinkableRowHead::Iterator::operator--()
{
	m_node = m_node->m_left;
	return *this;
}

CrossLinkableRowHead::Iterator CrossLinkableRowHead::Iterator::operator--(int)
{
	CrossLinkableRowHead::Iterator t = *this;
	m_node = m_node->m_left;
	return t;
}

CrossLinkableNode* CrossLinkableRowHead::Iterator::Node()
{
	return m_node;
}
