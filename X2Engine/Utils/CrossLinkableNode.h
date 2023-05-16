#pragma once

class CrossLinkableNode
{
	friend class CrossLinkableColHead;
	friend class CrossLinkableRowHead;
private:
	CrossLinkableNode* m_left;
	CrossLinkableNode* m_right;
	CrossLinkableNode* m_top;
	CrossLinkableNode* m_bottom;
	CrossLinkableColHead* m_colHead;
	CrossLinkableRowHead* m_rowHead;
protected:
	CrossLinkableNode();
	virtual ~CrossLinkableNode();
	CrossLinkableColHead* ColHead();
	CrossLinkableRowHead* RowHead();
	void RemoveSelf();
};
class CrossLinkableColHead final
{
private:
	CrossLinkableNode m_head;
	CrossLinkableNode* m_end;
public:
	class Iterator final
	{
		friend class CrossLinkableColHead;
	private:
		CrossLinkableNode* m_node;
		Iterator(CrossLinkableNode* node);
		Iterator();
	public:
		~Iterator();
		bool IsValid();
		Iterator operator++();
		Iterator operator++(int);
		Iterator operator--();
		Iterator operator--(int);
		CrossLinkableNode* Node();
	};
	CrossLinkableColHead();
	virtual ~CrossLinkableColHead();

	void Add(CrossLinkableNode* node);
	void Remove(CrossLinkableNode* node);
	Iterator Remove(Iterator iterator);
	bool HaveNode();
	Iterator GetIterator();
};
class CrossLinkableRowHead final
{
private:
	CrossLinkableNode m_head;
	CrossLinkableNode* m_end;
public:
	class Iterator final
	{
		friend class CrossLinkableRowHead;
	private:
		CrossLinkableNode* m_node;
		Iterator(CrossLinkableNode* node);
	public:
		Iterator();
		~Iterator();
		bool IsValid();
		Iterator operator++();
		Iterator operator++(int);
		Iterator operator--();
		Iterator operator--(int);
		CrossLinkableNode* Node();
	};
	CrossLinkableRowHead();
	virtual ~CrossLinkableRowHead();

	bool HaveNode();
	void Add(CrossLinkableNode* node);
	void Remove(CrossLinkableNode* node);
	Iterator Remove(Iterator iterator);
	Iterator GetIterator();
};