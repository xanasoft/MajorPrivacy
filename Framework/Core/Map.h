/*
* Copyright (c) 2023-2025 David Xanatos, xanasoft.com
* All rights reserved.
*
* This file is part of MajorPrivacy.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
*/

#pragma once

#include "Framework.h"
#include "Array.h"
#include "SafeRef.h"
#include "Pair.h"

FW_NAMESPACE_BEGIN

template <typename K>
struct MapDefaultLess {
	bool operator()(const K& l, const K& r) const {
		return l < r;
	}
};

template <typename K, typename V, typename LESS = MapDefaultLess<K>, typename NEW = DefaultValue<V>>
class Map : public AbstractContainer
{
public:
	struct SRBTreeNode
	{
		SRBTreeNode(const K& Key, const V& Value) : key(Key), Value(Value) {}
		SRBTreeNode* parent = nullptr;
		SRBTreeNode* left = nullptr;
		SRBTreeNode* right = nullptr;
		unsigned char color = 0;
		K key;
		V Value;
	};

	Map(AbstractMemPool* pMem = nullptr) : AbstractContainer(pMem) {}
	Map(const Map& Other) : AbstractContainer(nullptr)	{ Assign(Other); }
	Map(Map&& Other) : AbstractContainer(nullptr)		{ Move(Other); }
	~Map()											{ DetachData(); }

	Map& operator=(const Map& Other)				{ if(m_ptr != Other.m_ptr || m_pMem != Other.m_pMem) Assign(Other); return *this; }
	Map& operator=(Map&& Other)						{ if(m_ptr != Other.m_ptr || m_pMem != Other.m_pMem) Move(Other); return *this; }

	const SafeRef<const V> operator[](const K& Key) const	{ return GetContrPtr(Key); }
	SafeRef<V> operator[](const K& Key)				{ return SetValuePtr(Key, nullptr).first; }

	size_t Count() const							{ return m_ptr ? m_ptr->count : 0; }
	size_t size() const								{ return Count(); } // for STL compatibility
	bool IsEmpty() const							{ return !m_ptr || m_ptr->count == 0; }
	bool empty() const								{ return IsEmpty(); } // for STL compatibility

	void Clear()									{ DetachData(); }
	void clear()									{ Clear(); } // for STL compatibility

	bool MakeExclusive()							{ if(m_ptr && m_ptr->Refs > 1) return MakeExclusive(nullptr); return true; }

	EInsertResult Assign(const Map& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) { // if the containers use different memory pools, we need to make a copy of the parent
			Clear();
			return Merge(Other);
		}

		AttachData(Other.m_ptr);
		return EInsertResult::eOK;
	}

	EInsertResult Move(Map& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) { // if the containers use different memory pools, we need to make a copy of the data
			Clear();
			return Merge(Other);
		}

		DetachData();
		m_ptr = Other.m_ptr; 
		Other.m_ptr = nullptr;
		return EInsertResult::eOK;
	}

	EInsertResult Merge(const Map& Other)
	{
		if (!MakeExclusive(nullptr))
			return EInsertResult::eNoMemory;

		for (auto I = Other.begin(); I != Other.end(); ++I) {
			if(!InsertValue(I.Key(), &I.Value())) 
				return EInsertResult::eNoMemory;
		}
		return EInsertResult::eOK;
	}

	Array<K> Keys() const // Keys may repeat if there are multiple entries with the same Key
	{
		Array<K> Keys(m_pMem);
		if(!m_ptr || !Keys.Reserve(Count()))
			return Keys;
		for (SRBTreeNode* pNode = rbtree_first(m_ptr); pNode != &m_ptr->NullNode; pNode = rbtree_next(m_ptr, pNode)) {
			Keys.Append(pNode->Key);
		}
		return Keys;
	}

	V* GetContrPtr(const K& Key/*, size_t Index = 0*/) const	
	{ 
		SRBTreeNode* pNode = rbtree_search(m_ptr, Key/*, Index*/);
		return pNode ? &pNode->Value : nullptr;
	}
	V* GetValuePtr(const K& Key, bool bCanAdd = false/*, size_t Index = 0*/) 
	{ 
		if (!MakeExclusive(nullptr)) 
			return nullptr; 

		SRBTreeNode* pNode = rbtree_search(m_ptr, Key/*, Index*/);
		if (pNode) 
			return &pNode->Value;
		if(!bCanAdd)
			return nullptr;
		pNode = InsertValue(Key, nullptr);
		return pNode ? &pNode->Value : nullptr;
	}
	const SafeRef<const V> FindValue(const K& Key) const { return GetContrPtr(Key); }
	SafeRef<const V> FindValue(const K& Key)  { return GetContrPtr(Key); }
	SafeRef<V> GetOrAddValue(const K& Key) { return SetValuePtr(Key, nullptr).first; }

	Pair<V*, bool> SetValuePtr(const K& Key, const V* pValue) 
	{
		if(!MakeExclusive(nullptr)) 
			return MakePair<V*, bool>(nullptr, false);

		SRBTreeNode* pNode = rbtree_search(m_ptr, Key);
		if (pNode) {
			if (pValue) pNode->Value = *pValue;
			return MakePair(&pNode->Value, false);
		}

		pNode = InsertValue(Key, pValue);
		if(!pNode) return MakePair<V*, bool>(nullptr, false);
		return MakePair(&pNode->Value, true); // inserted true
	}
	V* AddValuePtr(const K& Key, const V* pValue)
	{
		if(!MakeExclusive(nullptr)) 
			return nullptr;

		return &InsertValue(Key, pValue)->Value;
	}
	bool SetValue(const K& Key, const V& Value)		{ return SetValuePtr(Key, &Value).second;}

	EInsertResult Insert(const K& Key, const V& Value, EInsertMode Mode = EInsertMode::eNormal)
	{ 
		if (!MakeExclusive(nullptr))
			return EInsertResult::eNoMemory;

		if (Mode != EInsertMode::eMulti) {
			SRBTreeNode* pNode = rbtree_search(m_ptr, Key);
			if (pNode) {
				if (Mode == EInsertMode::eNoReplace) 
					return EInsertResult::eKeyExists;
				pNode->Value = Value;
				return EInsertResult::eOK;
			}
		}

		if(Mode == EInsertMode::eNoInsert)
			return EInsertResult::eNoEntry;

		return InsertValue(Key, &Value) ? EInsertResult::eOK : EInsertResult::eNoMemory;
	}

	bool Remove(const K& Key/*, bool bAll = true*/)
	{
		if (!MakeExclusive(nullptr))
			return false;

		SRBTreeNode* pNode = rbtree_delete(m_ptr, Key);
		if (pNode) {
			pNode->~SRBTreeNode();
			MemFree(pNode);
			return true;
		}
		return false;
	}

	bool Remove(const K& Key, const V& Value/*, bool bAll = true*/)
	{
		if (!MakeExclusive(nullptr))
			return false;

		for(auto I = find(Key); I != end() && I.Key() == Key; ++I) {
			if(I.Value() == Value) {
				erase(I);
				return true;
			}
		}
		return false;
	}

	V Take(const K& Key)
	{
		V Value;
		if (MakeExclusive()) {
			SRBTreeNode* pNode = rbtree_delete(m_ptr, Key);
			if (pNode) {
				Value = pNode->Value;
				pNode->~SRBTreeNode();
				MemFree(pNode);
			}
		}
		return Value;
	}

#if 0

	// Support for range-based for loops
	// Template-based iterator supporting const and reverse iteration
	template <typename KeyType, typename ValueType, bool IsReverse = false>
	class MapIterator {
	public:
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = ValueType;
		using difference_type = ptrdiff_t;
		using pointer = ValueType*;
		using reference = ValueType&;

		MapIterator(const Map* pMap, SRBTreeNode* pNode) : pMap(pMap), pNode(pNode) {}

		reference operator*() const { return pNode->Value; }
		pointer operator->() const { return &pNode->Value; }
		reference Value() const { return pNode->Value; }
		const KeyType& Key() const { return pNode->key; }

		MapIterator& operator++() {
			if (pNode != &pMap->m_ptr->NullNode)
				pNode = IsReverse ? pMap->rbtree_previous(pMap->m_ptr, pNode) : pMap->rbtree_next(pMap->m_ptr, pNode);
			return *this;
		}

		MapIterator operator++(int) {
			MapIterator tmp = *this;
			++(*this);
			return tmp;
		}

		MapIterator& operator--() {
			if (pNode != &pMap->m_ptr->NullNode)
				pNode = IsReverse ? pMap->rbtree_next(pMap->m_ptr, pNode) : pMap->rbtree_previous(pMap->m_ptr, pNode);
			return *this;
		}

		MapIterator operator--(int) {
			MapIterator tmp = *this;
			--(*this);
			return tmp;
		}

		bool operator==(const MapIterator& other) const { return pNode == other.pNode; }
		bool operator!=(const MapIterator& other) const { return pNode != other.pNode; }

	protected:
		friend Map;
		const Map* pMap;
		SRBTreeNode* pNode;
	};

	// Legacy type for compatibility
	using Iterator = MapIterator<K, V, false>;

	// STL-compatible type aliases
	using iterator = MapIterator<K, V, false>;
	using const_iterator = MapIterator<const K, const V, false>;
	using reverse_iterator = MapIterator<K, V, true>;
	using const_reverse_iterator = MapIterator<const K, const V, true>;

	// Forward iteration
	iterator begin() { return m_ptr ? iterator(this, rbtree_first(m_ptr)) : end(); }
	const_iterator begin() const { return m_ptr ? const_iterator(this, rbtree_first(m_ptr)) : end(); }
	const_iterator cbegin() const { return m_ptr ? const_iterator(this, rbtree_first(m_ptr)) : cend(); }

	iterator end() { return iterator(this, &m_ptr->NullNode); }
	const_iterator end() const { return const_iterator(this, &m_ptr->NullNode); }
	const_iterator cend() const { return const_iterator(this, &m_ptr->NullNode); }

	// Reverse iteration
	reverse_iterator rbegin() { return m_ptr ? reverse_iterator(this, rbtree_last(m_ptr)) : rend(); }
	const_reverse_iterator rbegin() const { return m_ptr ? const_reverse_iterator(this, rbtree_last(m_ptr)) : rend(); }
	const_reverse_iterator crbegin() const { return m_ptr ? const_reverse_iterator(this, rbtree_last(m_ptr)) : crend(); }

	reverse_iterator rend() { return reverse_iterator(this, &m_ptr->NullNode); }
	const_reverse_iterator rend() const { return const_reverse_iterator(this, &m_ptr->NullNode); }
	const_reverse_iterator crend() const { return const_reverse_iterator(this, &m_ptr->NullNode); }

	// Legacy method for backwards compatibility
	Iterator begin(bool bReverse) const { return bReverse ? Iterator(this, m_ptr ? rbtree_last(m_ptr) : &m_ptr->NullNode) : Iterator(this, m_ptr ? rbtree_first(m_ptr) : &m_ptr->NullNode); }

#else

	// Support for range-based for loops
	class Iterator {
	public:
		Iterator(const Map* pMap, SRBTreeNode* pNode, bool bReverse = false) : pMap(pMap), pNode(pNode), bReverse(bReverse) {}

		Iterator& operator++() { if (pNode != &pMap->m_ptr->NullNode) pNode = bReverse ? pMap->rbtree_previous(pMap->m_ptr, pNode) : pMap->rbtree_next(pMap->m_ptr, pNode); return *this;  }

		V& operator*() const { return pNode->Value; }
		V* operator->() const { return &pNode->Value; }
		V& Value() const { return pNode->Value; }
		const K& Key() const { return pNode->key; }

		bool operator!=(const Iterator& other) const { return pNode != other.pNode; }
		bool operator==(const Iterator& other) const { return pNode == other.pNode; }

	protected:
		friend Map;

		const Map* pMap;
		SRBTreeNode* pNode;
		bool bReverse;
	};

	Iterator begin(bool bReverse = false) const		{ return m_ptr ? Iterator(this, bReverse ? rbtree_last(m_ptr) : rbtree_first(m_ptr), bReverse) : end(); }
	Iterator end() const							{ return Iterator(this, &m_ptr->NullNode); }

#endif

	Iterator find(const K& Key) const
	{
		if(!m_ptr) return end();
		SRBTreeNode* pNode = rbtree_search(m_ptr, Key);
		if (pNode)
			return Iterator(this, pNode);
		return end();
	}

	bool contains(const K& Key) const
	{
		if(!m_ptr) return false;
		return rbtree_search(m_ptr, Key) != nullptr;
	}

	Iterator find_le(const K& Key, bool bOrEqual = true) const
	{
		if(!m_ptr) return end();

		SRBTreeNode* pNode;
		if(rbtree_find_less_equal(m_ptr, Key, &pNode) && !bOrEqual)
			pNode = rbtree_previous(m_ptr, pNode);
		if (pNode != &m_ptr->NullNode)
			return Iterator(this, pNode);
		return end();
	}

	Iterator find_ge(const K& Key, bool bOrEqual = true) const
	{
		if(!m_ptr) return end();

		SRBTreeNode* pNode;
		if(rbtree_find_greater_equal(m_ptr, Key, &pNode) && !bOrEqual)
			pNode = rbtree_next(m_ptr, pNode);
		if (pNode != &m_ptr->NullNode)
			return Iterator(this, pNode);
		return end();
	}

	// like in stl
	Iterator lower_bound(const K& Key) const		{ return find_ge(Key); }
	Iterator upper_bound(const K& Key) const		{ return find_ge(Key, false); }

	Iterator erase(Iterator I)
	{
		if (!I.pNode || !MakeExclusive(&I))
			return end();
		SRBTreeNode* pNode = I.pNode;
		++I;
		if (pNode != &m_ptr->NullNode) {
			rbtree_delete(m_ptr, pNode);
			pNode->~SRBTreeNode();
			MemFree(pNode);
		}
		return I;
	}

protected:
	struct SMapData
	{
		SMapData(const V& Null) : NullNode(K(), Null) {}
		volatile LONG Refs = 0;
		size_t count = 0;
		SRBTreeNode* root = nullptr;
		SRBTreeNode NullNode;
	}* m_ptr = nullptr;

	bool MakeExclusive(Iterator* pI)
	{
		if (!m_ptr) 
		{
			SMapData* ptr = MakeData();
			AttachData(ptr);
			return ptr != nullptr;
		}
		else if (m_ptr->Refs > 1)
		{
			SMapData* ptr = MakeData();
			if (!ptr) return false;
			bool bSuccess = true;
			for (SRBTreeNode* pNode = rbtree_first(m_ptr); pNode != &m_ptr->NullNode; pNode = rbtree_next(m_ptr, pNode)) {
				SRBTreeNode* pNew = (SRBTreeNode*)MemAlloc(sizeof(SRBTreeNode));
				if (!pNew) {
					bSuccess = false;
					break;
				}
				new (pNew) SRBTreeNode(pNode->key, pNode->Value);
				rbtree_insert(ptr, pNew);
				// we need to update the iterator if it points to the current entry
				if (pI && pI->pNode == pNode) {
					pI->pNode = pNew;
					pI = nullptr;
				}
			}
			if (!bSuccess) {
				// Clean up partial allocation
				for (SRBTreeNode* node = ptr->root; node != &ptr->NullNode;) {
					if (node->right != &ptr->NullNode) {
						SRBTreeNode* tmpNode = node->right;
						node->right = tmpNode->left;
						tmpNode->left = node;
						node = tmpNode;
					}
					else {
						SRBTreeNode* tmpNode = node->left;
						node->~SRBTreeNode();
						MemFree(node);
						node = tmpNode;
					}
				}
				MemFree(ptr);
				return false;
			}
			AttachData(ptr);
		}
		return true;
	}

	SRBTreeNode* InsertValue(const K& Key, const V* pValue)
	{
		ASSERT(m_ptr);

		SRBTreeNode* pNode = (SRBTreeNode*)MemAlloc(sizeof(SRBTreeNode));
		if (!pNode) return nullptr;
		if(pValue) 
			new (pNode) SRBTreeNode(Key, *pValue);
		else
		{
			NEW New;
			new (pNode) SRBTreeNode(Key, New(m_pMem));
		}

		rbtree_insert(m_ptr, pNode);

		return pNode;
	}

#define RBTREE_NULL &rbtree->NullNode

#define rbtree_t SMapData
#define rbnode_t SRBTreeNode

	////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// base on rbtree.c by NLnet Labs, Copyright (c) 2001-2007. All rights reserved.
	// 
	// This code is open source under the BSD license. 
	//

#define log_assert(x)

	/** Node colour black */
#define	BLACK	0
	/** Node colour red */
#define	RED	1

	/*
	* Rotates the node to the left.
	*
	*/
	void rbtree_rotate_left(rbtree_t *rbtree, rbnode_t *node)
	{
		rbnode_t *right = node->right;
		node->right = right->left;
		if (right->left != RBTREE_NULL)
			right->left->parent = node;

		right->parent = node->parent;

		if (node->parent != RBTREE_NULL) {
			if (node == node->parent->left) {
				node->parent->left = right;
			} else  {
				node->parent->right = right;
			}
		} else {
			rbtree->root = right;
		}
		right->left = node;
		node->parent = right;
	}

	/*
	* Rotates the node to the right.
	*
	*/
	void rbtree_rotate_right(rbtree_t *rbtree, rbnode_t *node)
	{
		rbnode_t *left = node->left;
		node->left = left->right;
		if (left->right != RBTREE_NULL)
			left->right->parent = node;

		left->parent = node->parent;

		if (node->parent != RBTREE_NULL) {
			if (node == node->parent->right) {
				node->parent->right = left;
			} else  {
				node->parent->left = left;
			}
		} else {
			rbtree->root = left;
		}
		left->right = node;
		node->parent = left;
	}

	void rbtree_insert_fixup(rbtree_t *rbtree, rbnode_t *node)
	{
		rbnode_t	*uncle;

		/* While not at the root and need fixing... */
		while (node != rbtree->root && node->parent->color == RED) {
			/* If our parent is left child of our grandparent... */
			if (node->parent == node->parent->parent->left) {
				uncle = node->parent->parent->right;

				/* If our uncle is red... */
				if (uncle->color == RED) {
					/* Paint the parent and the uncle black... */
					node->parent->color = BLACK;
					uncle->color = BLACK;

					/* And the grandparent red... */
					node->parent->parent->color = RED;

					/* And continue fixing the grandparent */
					node = node->parent->parent;
				} else {				/* Our uncle is black... */
					/* Are we the right child? */
					if (node == node->parent->right) {
						node = node->parent;
						rbtree_rotate_left(rbtree, node);
					}
					/* Now we're the left child, repaint and rotate... */
					node->parent->color = BLACK;
					node->parent->parent->color = RED;
					rbtree_rotate_right(rbtree, node->parent->parent);
				}
			} else {
				uncle = node->parent->parent->left;

				/* If our uncle is red... */
				if (uncle->color == RED) {
					/* Paint the parent and the uncle black... */
					node->parent->color = BLACK;
					uncle->color = BLACK;

					/* And the grandparent red... */
					node->parent->parent->color = RED;

					/* And continue fixing the grandparent */
					node = node->parent->parent;
				} else {				/* Our uncle is black... */
					/* Are we the right child? */
					if (node == node->parent->left) {
						node = node->parent;
						rbtree_rotate_right(rbtree, node);
					}
					/* Now we're the right child, repaint and rotate... */
					node->parent->color = BLACK;
					node->parent->parent->color = RED;
					rbtree_rotate_left(rbtree, node->parent->parent);
				}
			}
		}
		rbtree->root->color = BLACK;
	}


	/*
	* Inserts a node into a red black tree.
	*
	*/
	void rbtree_insert (rbtree_t *rbtree, rbnode_t *data)
	{
		bool less = false;

		/* We start at the root of the tree */
		rbnode_t	*node = rbtree->root;
		rbnode_t	*parent = RBTREE_NULL;

		/* Lets find the new parent... */
		while (node != RBTREE_NULL) {
			less = Less(data->key, node->key);
			parent = node;

			if (less) {
				node = node->left;
			} else {
				node = node->right;
			}
		}

		/* Initialize the new node */
		data->parent = parent;
		data->left = data->right = RBTREE_NULL;
		data->color = RED;
		rbtree->count++;

		/* Insert it into the tree... */
		if (parent != RBTREE_NULL) {
			if (less) {
				parent->left = data;
			} else {
				parent->right = data;
			}
		} else {
			rbtree->root = data;
		}

		/* Fix up the red-black properties... */
		rbtree_insert_fixup(rbtree, data);
	}

	/*
	* Searches the red black tree, returns the data if key is found or NULL otherwise.
	*
	*/
	rbnode_t* rbtree_search (rbtree_t *rbtree, const K& key) const
	{
		rbnode_t *node;

		if (rbtree && rbtree_find_less_equal(rbtree, key, &node)) {
			return node;
		} else {
			return nullptr;
		}
	}

	/** helpers for delete: swap node colours */
	void swap_int8(unsigned char* x, unsigned char* y) 
	{ 
		unsigned char t = *x; *x = *y; *y = t; 
	}

	/** helpers for delete: swap node pointers */
	void swap_np(rbnode_t** x, rbnode_t** y) 
	{
		rbnode_t* t = *x; *x = *y; *y = t; 
	}

	/** Update parent pointers of child trees of 'parent' */
	void change_parent_ptr(rbtree_t* rbtree, rbnode_t* parent, rbnode_t* old, rbnode_t* neu)
	{
		if(parent == RBTREE_NULL)
		{
			log_assert(rbtree->root == old);
			if(rbtree->root == old) rbtree->root = neu;
			return;
		}
		log_assert(parent->left == old || parent->right == old
			|| parent->left == neu || parent->right == neu);
		if(parent->left == old) parent->left = neu;
		if(parent->right == old) parent->right = neu;
	}

	/** Update parent pointer of a node 'child' */
	void change_child_ptr(rbtree_t* rbtree, rbnode_t* child, rbnode_t* old, rbnode_t* neu)
	{
		if(child == RBTREE_NULL) return;
		log_assert(child->parent == old || child->parent == neu);
		if(child->parent == old) child->parent = neu;
	}

	rbnode_t* rbtree_delete(rbtree_t *rbtree, const K& key)
	{
		rbnode_t *to_delete;
		if((to_delete = rbtree_search(rbtree, key)) == 0) return 0;

		rbtree_delete(rbtree, to_delete);
		return to_delete;
	}

	void rbtree_delete(rbtree_t *rbtree, rbnode_t *to_delete)
	{
		rbnode_t *child;

		rbtree->count--;

		/* make sure we have at most one non-leaf child */
		if(to_delete->left != RBTREE_NULL && to_delete->right != RBTREE_NULL)
		{
			/* swap with smallest from right subtree (or largest from left) */
			rbnode_t *smright = to_delete->right;
			while(smright->left != RBTREE_NULL)
				smright = smright->left;
			/* swap the smright and to_delete elements in the tree,
			* but the rbnode_t is first part of user data struct
			* so cannot just swap the keys and data pointers. Instead
			* readjust the pointers left,right,parent */

			/* swap colors - colors are tied to the position in the tree */
			swap_int8(&to_delete->color, &smright->color);

			/* swap child pointers in parents of smright/to_delete */
			change_parent_ptr(rbtree, to_delete->parent, to_delete, smright);
			if(to_delete->right != smright)
				change_parent_ptr(rbtree, smright->parent, smright, to_delete);

			/* swap parent pointers in children of smright/to_delete */
			change_child_ptr(rbtree, smright->left, smright, to_delete);
			change_child_ptr(rbtree, smright->left, smright, to_delete);
			change_child_ptr(rbtree, smright->right, smright, to_delete);
			change_child_ptr(rbtree, smright->right, smright, to_delete);
			change_child_ptr(rbtree, to_delete->left, to_delete, smright);
			if(to_delete->right != smright)
				change_child_ptr(rbtree, to_delete->right, to_delete, smright);
			if(to_delete->right == smright)
			{
				/* set up so after swap they work */
				to_delete->right = to_delete;
				smright->parent = smright;
			}

			/* swap pointers in to_delete/smright nodes */
			swap_np(&to_delete->parent, &smright->parent);
			swap_np(&to_delete->left, &smright->left);
			swap_np(&to_delete->right, &smright->right);

			/* now delete to_delete (which is at the location where the smright previously was) */
		}
		log_assert(to_delete->left == RBTREE_NULL || to_delete->right == RBTREE_NULL);

		if(to_delete->left != RBTREE_NULL) child = to_delete->left;
		else child = to_delete->right;

		/* unlink to_delete from the tree, replace to_delete with child */
		change_parent_ptr(rbtree, to_delete->parent, to_delete, child);
		change_child_ptr(rbtree, child, to_delete, to_delete->parent);

		if(to_delete->color == RED)
		{
			/* if node is red then the child (black) can be swapped in */
		}
		else if(child->color == RED)
		{
			/* change child to BLACK, removing a RED node is no problem */
			if(child!=RBTREE_NULL) child->color = BLACK;
		}
		else rbtree_delete_fixup(rbtree, child, to_delete->parent);

		/* unlink completely */
		to_delete->parent = RBTREE_NULL;
		to_delete->left = RBTREE_NULL;
		to_delete->right = RBTREE_NULL;
		to_delete->color = BLACK;
	}

	void rbtree_delete_fixup(rbtree_t* rbtree, rbnode_t* child, rbnode_t* child_parent)
	{
		rbnode_t* sibling;
		int go_up = 1;

		/* determine sibling to the node that is one-black short */
		if(child_parent->right == child) sibling = child_parent->left;
		else sibling = child_parent->right;

		while(go_up)
		{
			if(child_parent == RBTREE_NULL)
			{
				/* removed parent==black from root, every path, so ok */
				return;
			}

			if(sibling->color == RED)
			{	/* rotate to get a black sibling */
				child_parent->color = RED;
				sibling->color = BLACK;
				if(child_parent->right == child)
					rbtree_rotate_right(rbtree, child_parent);
				else	rbtree_rotate_left(rbtree, child_parent);
				/* new sibling after rotation */
				if(child_parent->right == child) sibling = child_parent->left;
				else sibling = child_parent->right;
			}

			if(child_parent->color == BLACK 
				&& sibling->color == BLACK
				&& sibling->left->color == BLACK
				&& sibling->right->color == BLACK)
			{	/* fixup local with recolor of sibling */
				if(sibling != RBTREE_NULL)
					sibling->color = RED;

				child = child_parent;
				child_parent = child_parent->parent;
				/* prepare to go up, new sibling */
				if(child_parent->right == child) sibling = child_parent->left;
				else sibling = child_parent->right;
			}
			else go_up = 0;
		}

		if(child_parent->color == RED
			&& sibling->color == BLACK
			&& sibling->left->color == BLACK
			&& sibling->right->color == BLACK) 
		{
			/* move red to sibling to rebalance */
			if(sibling != RBTREE_NULL)
				sibling->color = RED;
			child_parent->color = BLACK;
			return;
		}
		log_assert(sibling != RBTREE_NULL);

		/* get a new sibling, by rotating at sibling. See which child
		of sibling is red */
		if(child_parent->right == child
			&& sibling->color == BLACK
			&& sibling->right->color == RED
			&& sibling->left->color == BLACK)
		{
			sibling->color = RED;
			sibling->right->color = BLACK;
			rbtree_rotate_left(rbtree, sibling);
			/* new sibling after rotation */
			if(child_parent->right == child) sibling = child_parent->left;
			else sibling = child_parent->right;
		}
		else if(child_parent->left == child
			&& sibling->color == BLACK
			&& sibling->left->color == RED
			&& sibling->right->color == BLACK)
		{
			sibling->color = RED;
			sibling->left->color = BLACK;
			rbtree_rotate_right(rbtree, sibling);
			/* new sibling after rotation */
			if(child_parent->right == child) sibling = child_parent->left;
			else sibling = child_parent->right;
		}

		/* now we have a black sibling with a red child. rotate and exchange colors. */
		sibling->color = child_parent->color;
		child_parent->color = BLACK;
		if(child_parent->right == child)
		{
			log_assert(sibling->left->color == RED);
			sibling->left->color = BLACK;
			rbtree_rotate_right(rbtree, child_parent);
		}
		else
		{
			log_assert(sibling->right->color == RED);
			sibling->right->color = BLACK;
			rbtree_rotate_left(rbtree, child_parent);
		}
	}

	int rbtree_find_less_equal(rbtree_t *rbtree, const K& key, rbnode_t **result) const
	{
		rbnode_t *node;

		log_assert(result);

		/* We start at root... */
		node = rbtree->root;

		*result = nullptr;

		/* While there are children... */
		while (node != RBTREE_NULL) {
			if (Less(key, node->key)) {
				/* Greater */
				node = node->left;
			} else if(Less(node->key, key)) {
				/* Less (Temporary match) */
				*result = node;
				node = node->right;
			} else {
				/* Exact match */
				*result = node;
				return 1;
			}
		}
		return 0;
	}

	int rbtree_find_greater_equal(rbtree_t *rbtree, const K& key, rbnode_t **result) const
	{
		rbnode_t *node;

		log_assert(result);

		/* We start at root... */
		node = rbtree->root;

		*result = nullptr;

		/* While there are children... */
		while (node != RBTREE_NULL) {
			if (Less(key, node->key)) {
				/* Greater */
				*result = node;
				return 0;
			} else if (Less(node->key, key)) {
				/* Less */
				node = node->right;
			} else {
				/* Exact match */
				*result = node;
				return 1;
			}
		}
		return 0;
	}

	/*
	* Finds the first element in the red black tree
	*
	*/
	rbnode_t *rbtree_first (rbtree_t *rbtree) const
	{
		rbnode_t *node;

		for (node = rbtree->root; node->left != RBTREE_NULL; node = node->left);
		return node;
	}

	rbnode_t *rbtree_last (rbtree_t *rbtree) const
	{
		rbnode_t *node;

		for (node = rbtree->root; node->right != RBTREE_NULL; node = node->right);
		return node;
	}

	/*
	* Returns the next node...
	*
	*/
	rbnode_t *rbtree_next (rbtree_t* rbtree, rbnode_t *node) const
	{
		rbnode_t *parent;

		if (node->right != RBTREE_NULL) {
			/* One right, then keep on going left... */
			for (node = node->right; node->left != RBTREE_NULL; node = node->left);
		} else {
			parent = node->parent;
			while (parent != RBTREE_NULL && node == parent->right) {
				node = parent;
				parent = parent->parent;
			}
			node = parent;
		}
		return node;
	}

	rbnode_t *rbtree_previous(rbtree_t* rbtree, rbnode_t *node) const
	{
		rbnode_t *parent;

		if (node->left != RBTREE_NULL) {
			/* One left, then keep on going right... */
			for (node = node->left; node->right != RBTREE_NULL; node = node->right);
		} else {
			parent = node->parent;
			while (parent != RBTREE_NULL && node == parent->left) {
				node = parent;
				parent = parent->parent;
			}
			node = parent;
		}
		return node;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool Less(const K& l, const K& r) const
	{
		LESS Less;
		return Less(l, r);
	}

	SMapData* MakeData()
	{
		SMapData* ptr = (SMapData*)MemAlloc(sizeof(SMapData));
		if(!ptr) 
			return nullptr;
		NEW New;
		new (ptr) SMapData(New(m_pMem));
		ptr->NullNode.parent = ptr->NullNode.left = ptr->NullNode.right = &ptr->NullNode;
		ptr->NullNode.color = BLACK;
		ptr->root = &ptr->NullNode;
		return ptr;
	}

	void AttachData(SMapData* ptr)
	{
		DetachData();

		if (ptr) {
			m_ptr = ptr;
			InterlockedIncrement(&m_ptr->Refs);
		}
	}

	void DetachData()
	{
		if (m_ptr) {
			if (InterlockedDecrement(&m_ptr->Refs) == 0) {
				for (SRBTreeNode* node = m_ptr->root; node != &m_ptr->NullNode;) {
					if (node->right != &m_ptr->NullNode) {
						SRBTreeNode* tmpNode = node->right;
						node->right = tmpNode->left;
						tmpNode->left = node;
						node = tmpNode;
					}
					else {
						SRBTreeNode* tmpNode = node->left;
						node->~SRBTreeNode();
						MemFree(node);
						node = tmpNode;
					}
				}
				MemFree(m_ptr);
			}
			m_ptr = nullptr;
		}
	}
};

FW_NAMESPACE_END