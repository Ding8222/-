#include <functional>
#include <deque>
#include <assert.h>

using namespace std;

class CNode;
class CTask;

enum eStatus
{
	BH_INVALID,
	BH_SUCCESS,
	BH_FAILURE,
	BH_RUNNING,
	BH_ABORTED,
	BH_SUSPENDED,
};

typedef std::function<void(eStatus)> BehaviorObserver;

// �ڵ����
class CNode
{
public:
	virtual CTask *Create() = 0;
	virtual void Destroy(CTask *) = 0;

	virtual ~CNode() {}
};

// ��Ϊ����
class CTask
{
public:
	CTask(CNode &node) :
		m_Node(&node)
	{
	}

	virtual ~CTask() {}

	virtual eStatus Update() = 0;

	virtual void OnInitialize() {}
	virtual void OnTerminate(eStatus) {}

protected:
	CNode * m_Node;
};

// Node�����ڴ�ִ��
class CBehavior
{
public:
	CBehavior() :
		m_Task(nullptr),
		m_Node(nullptr),
		m_Status(BH_INVALID)
	{
	}

	CBehavior(CNode &node) :
		m_Task(nullptr),
		m_Node(nullptr),
		m_Status(BH_INVALID)
	{
		Setup(node);
	}

	~CBehavior()
	{
		m_Status = BH_INVALID;
		Teardown();
	}

	void Setup(CNode &node)
	{
		Teardown();

		m_Node = &node;
		m_Task = node.Create();
	}

	void Teardown()
	{
		if (m_Task == nullptr)
		{
			return;
		}

		assert(m_Status != BH_RUNNING);
		m_Node->Destroy(m_Task);
		m_Task = nullptr;
	}

	eStatus Tick()
	{
		if (m_Status != BH_RUNNING)
		{
			m_Task->OnInitialize();
		}

		m_Status = m_Task->Update();

		if (m_Status != BH_RUNNING)
		{
			m_Task->OnTerminate(m_Status);
		}

		return m_Status;
	}

	void Rest()
	{
		m_Status = BH_INVALID;
	}

	void Abort()
	{
		m_Task->OnTerminate(BH_ABORTED);
		m_Status = BH_ABORTED;
	}

	bool IsTerminated() const
	{
		return m_Status == BH_SUCCESS || m_Status == BH_FAILURE;
	}

	bool IsRunning() const
	{
		return m_Status == BH_RUNNING;
	}

	eStatus GetStatus() const
	{
		return m_Status;
	}

	// ��ȡ�ýڵ���Ϊ
	template <class TASK>
	TASK *Get()const
	{
		return dynamic_cast<TASK *>(m_Task);
	}

	CTask * m_Task;
	CNode *m_Node;
	eStatus m_Status;
	BehaviorObserver m_Observer;
};

// Ԥ����һƬk_MaxBehaviorTreeMemory��С��m_Buffer
// ÿ�η����ʱ��,��m_Bufferȡ��һ�����ڴ�
const size_t k_MaxBehaviorTreeMemory = 8192;
class CBehaviorAllocate
{
public:
	CBehaviorAllocate() :
		m_Buffer(new uint8_t[k_MaxBehaviorTreeMemory]),
		m_Offset(0)
	{
	}

	~CBehaviorAllocate()
	{
		delete[] m_Buffer;
	}

	template <typename T>
	T &allocate()
	{
		T *node = new ((void *)((uintptr_t)m_Buffer + m_Offset)) T;
		m_Offset += sizeof(T);
		assert(m_Offset < k_MaxBehaviorTreeMemory);
		return *node;
	}

protected:
	uint8_t * m_Buffer;
	size_t m_Offset;
};

class CBehaviorTree
{
public:
	void Start(CBehavior &n, BehaviorObserver *observer = nullptr)
	{
		if (observer != nullptr)
		{
			n.m_Observer = *observer;
		}
		m_Behaviors.push_front(&n);
	}

	void Stop(CBehavior &n, eStatus result)
	{
		assert(result != BH_RUNNING);
		n.m_Status = result;

		if (n.m_Observer)
		{
			n.m_Observer(result);
		}
	}

	void Tick()
	{
		m_Behaviors.push_back(nullptr);

		while (Step())
		{
			continue;
		}
	}

	bool Step()
	{
		CBehavior *current = m_Behaviors.front();
		m_Behaviors.pop_front();

		if (current == nullptr)
		{
			return false;
		}

		current->Tick();

		if (current->m_Status != BH_RUNNING && current->m_Observer)
		{
			current->m_Observer(current->m_Status);
		}
		else
		{
			m_Behaviors.push_back(current);
		}
		return true;
	}

protected: 
	std::deque<CBehavior *> m_Behaviors;
};

// ��Ϊ����
struct CMockTask :public CTask
{
	int m_InitializeCalled;
	int m_TerminateCalled;
	int m_UpdateCalled;
	eStatus m_ReturnStatus;
	eStatus m_TerminateStatus;

	CMockTask(CNode &node) :
		CTask(node),
		m_InitializeCalled(0),
		m_TerminateCalled(0),
		m_UpdateCalled(0),
		m_ReturnStatus(BH_RUNNING),
		m_TerminateStatus(BH_INVALID)
	{
	}

	virtual void OnInitialize()
	{
		++m_InitializeCalled;
	}

	virtual void OnTerminate(eStatus s)
	{
		++m_TerminateCalled;
		m_TerminateStatus = s;
	}

	virtual eStatus Update()
	{
		++m_UpdateCalled;
		return m_ReturnStatus;
	}
};

// �ڵ㹤��
struct CMockNode :public CNode
{
	virtual void Destroy(CTask *) {}
	virtual CTask *Create()
	{
		m_Task = new CMockTask(*this);
		return m_Task;
	}

	virtual ~CMockNode()
	{
		delete m_Task;
	}

	CMockNode() :
		m_Task(nullptr)
	{
	}

	CMockTask *m_Task;
};

void test()
{
	CBehaviorAllocate t;
	CMockNode &n = t.allocate<CMockNode>();
	CBehavior &b = t.allocate<CBehavior>();
	b.Setup(n);

	CBehaviorTree bt;
	bt.Start(b);
	bt.Tick();
}

// װ�νڵ㣬����ֻ��һ���ڵ�
class CDecorator :public CNode
{
public:
	CDecorator(CNode *child) :m_Child(child) {}
	CNode & GetChild() { return *m_Child; }

protected:
	CNode * m_Child;
};

// װ�νڵ㹤����
template <class TASK>
class CMockDecorator :public CDecorator
{
public:
	CMockDecorator(CNode *child) :
		CDecorator(child)
	{

	}

	virtual CTask *Create()
	{
		return new TASK(*this);
	}

	virtual void Destroy(CTask *task)
	{
		delete task;
	}
};

// �ظ�
class CRepeat :public CTask
{
public:
	CRepeat(CDecorator &node) :CTask(node) {}

	CDecorator &GetNode()
	{
		return *static_cast<CDecorator*>(m_Node);
	}

	void SetCount(int count)
	{
		m_Limit = count;
	}

	virtual void OnInitialize()
	{
		m_Counter = 0;
		m_Behavior.Setup(GetNode().GetChild());
	}

	virtual eStatus Update()
	{
		for (;;)
		{
			m_Behavior.Tick();
			if (m_Behavior.GetStatus() == BH_RUNNING) break;
			if (m_Behavior.GetStatus() == BH_FAILURE) return BH_FAILURE;
			if (++m_Counter == m_Limit) return BH_SUCCESS;
			m_Behavior.Rest();
		}
		return BH_INVALID;
	}

protected:
	int m_Limit;
	int m_Counter;
	CBehavior m_Behavior;
};

typedef CMockDecorator<CRepeat> CMockRepeat;

void testrepeat()
{
	CBehaviorTree bt;
	CBehaviorAllocate t;
	CMockNode &n = t.allocate<CMockNode>();
	CMockRepeat re(&n);
	CBehavior b(re);
	b.Get<CRepeat>()->SetCount(3);
	b.Tick();
}

const size_t k_MaxChildrenPerComposite = 7;
// ��Ͻڵ㣬ӵ�ж���ӽڵ�
class CComposite :public CNode
{
public:
	CComposite() :
		m_ChildCount(0),
		m_BehaviorTree(nullptr)
	{
	}

	void AddChild(CNode &child)
	{
		assert(m_ChildCount < k_MaxChildrenPerComposite);
		ptrdiff_t p = (uintptr_t)&child - (uintptr_t)this;
		assert(p < std::numeric_limits<uint16_t>::max());
		m_Children[m_ChildCount++] = static_cast<uint16_t>(p);
	}

	void AddChildFront(CNode &child)
	{
		assert(m_ChildCount < k_MaxChildrenPerComposite);
		ptrdiff_t p = (uintptr_t)&child - (uintptr_t)this;
		assert(p < std::numeric_limits<uint16_t>::max());

		for (uint16_t i = m_ChildCount; i > 0; --i)
		{
			m_Children[i] = m_Children[i - 1];
		}
		m_Children[0] = static_cast<uint16_t>(p);
	}

	CNode &GetChild(uint16_t index)
	{
		assert(index < m_ChildCount);
		return *(CNode*)((uintptr_t)this + m_Children[index]);
	}

	uint16_t GetChildCount() const
	{
		return m_ChildCount;
	}

public:
	uint16_t m_Children[k_MaxChildrenPerComposite];
	uint16_t m_ChildCount;
	CBehaviorTree *m_BehaviorTree;
};

// ��Ͻڵ㹤����
template <class TASK>
class CMockComposite :public CComposite
{
public:
	void Initialize(CBehaviorTree &bt, CBehaviorAllocate &tree, size_t size)
	{
		CComposite::m_BehaviorTree = &bt;
		for (size_t i = 0; i < size; ++i)
		{
			CMockNode &n = tree.allocate<CMockNode>();
			CComposite::AddChild(n);
		}
	}

	CMockTask &operator[](uint16_t index)
	{
		assert(index < CComposite::GetChildCount());
		CMockTask *task = static_cast<CMockNode *>(CComposite::GetChild(index))->m_Task;
		assert(task != nullptr);
		return *task;
	}

	virtual CTask *Create()
	{
		return new TASK(*this);
	}

	virtual void Destroy(CTask *task)
	{
		delete task;
	}

};

// ���нڵ�
// ˳������,��һ��ʧ��,�򷵻�
// Ҫ���������гɹ�,�򷵻سɹ�,���򷵻�ʧ��
class CSequence :public CTask
{
public:
	CSequence(CComposite &node) :
		CTask(node)
	{
		m_BehaviorTree = node.m_BehaviorTree;
	}

	CComposite &GetNode()
	{
		return *static_cast<CComposite *>(m_Node);
	}

	virtual void OnInitialize()
	{
		m_CurrentIndex = 0;
		m_CurrentBehavior.Setup(GetNode().GetChild(m_CurrentIndex));
		BehaviorObserver observer = std::bind(&CSequence::onChildComplete, this, std::placeholders::_1);
	}

	void onChildComplete(eStatus)
	{
		CBehavior &child = m_CurrentBehavior;
		if (child.m_Status == BH_FAILURE)
		{
			m_BehaviorTree->Stop(child, BH_FAILURE);
			return;
		}

		assert(child.m_Status == BH_SUCCESS);
		if (++m_CurrentIndex == GetNode().GetChildCount())
		{
			m_BehaviorTree->Stop(child, BH_SUCCESS);
		}
		else
		{
			BehaviorObserver observer = std::bind(&CSequence::onChildComplete, this, std::placeholders::_1);
			m_CurrentBehavior.Setup(GetNode().GetChild(m_CurrentIndex));
			m_BehaviorTree->Start(m_CurrentBehavior, &observer);
		}
	}

	virtual eStatus Update()
	{
		for (;;)
		{
			eStatus s = m_CurrentBehavior.Tick();

			if (s != BH_SUCCESS)
			{
				// ����ɹ��ˣ�����һ�������򷵻�
				return s;
			}

			if (++m_CurrentIndex == GetNode().GetChildCount())
			{
				// ���нڵ����гɹ�
				return BH_SUCCESS;
			}

			m_CurrentBehavior.Setup(GetNode().GetChild(m_CurrentIndex));
		}
	}

	CBehavior m_CurrentBehavior;
	uint16_t m_CurrentIndex;
	CBehaviorTree* m_BehaviorTree;
};

typedef CMockComposite<CSequence> CMockSequence;

void testsequence()
{
	CBehaviorTree bt;
	CBehaviorAllocate t;
	CMockSequence &se = t.allocate<CMockSequence>();

	se.Initialize(bt,t, 2);

	CBehavior b;
	b.Setup(se);

	bt.Start(b);
	bt.Tick();
}

// ѡ��׶�
// ˳������,��һ���ɹ���ֱ�ӷ���
// ֻҪ��һ�����гɹ�,�򷵻سɹ�,���򷵻�ʧ��
class CSelector :public CTask
{
public:
	CSelector(CComposite &node) :
		CTask(node)
	{
	}

protected:
	CComposite & GetNode()
	{
		return *static_cast<CComposite*>(m_Node);
	}

	virtual void OnInitialize()
	{
		m_CurrentIndex = 0;
		m_CurrentBehavior.Setup(GetNode().GetChild(m_CurrentIndex));
	}

	virtual eStatus Update()
	{
		for (;;)
		{
			eStatus s = m_CurrentBehavior.Tick();

			if (s != BH_FAILURE)
			{
				// ���ʧ���ˣ�����һ�������򷵻�
				return s;
			}

			if (++m_CurrentIndex == GetNode().GetChildCount())
			{
				// ���нڵ�����ʧ��
				return BH_FAILURE;
			}

			m_CurrentBehavior.Setup(GetNode().GetChild(m_CurrentIndex));
		}
	}

	CBehavior m_CurrentBehavior;
	uint16_t m_CurrentIndex;
};

typedef CMockComposite<CSelector> CMockSelector;

void testselector()
{
	CBehaviorTree bt;
	CBehaviorAllocate t;
	CMockSelector &se = t.allocate<CMockSelector>();
	se.Initialize(bt, t, 2);
	CBehavior b(se);
	bt.Start(b);
	bt.Tick();
}

// ���нڵ�
// ȫ������,������ɹ�����ʧ�ܸ���������ʱ��,����
// ʧ��Ӧ�����ڳɹ�
class CParallel :public CTask
{
public:
	enum ePolicy
	{
		RequireOne,	//������������
		RequireAll,	//ȫ����������
	};

	CParallel(CComposite &node) :
		CTask(node)
	{
	}


	// �ɹ�����,ʧ������
	CParallel(CComposite &node, ePolicy forSuccess, ePolicy forFailure) :
		m_SuccessPolicy(forSuccess),
		m_FailruePolicy(forFailure),
		CTask(node)
	{

	}

	void SetPolicy(ePolicy forSuccess, ePolicy forFailure)
	{
		m_SuccessPolicy = forSuccess;
		m_FailruePolicy = forFailure;
	}

	CComposite &GetNode()
	{
		return *static_cast<CComposite*>(m_Node);
	}

protected:
	virtual eStatus Update()
	{
		size_t nSuccessCount = 0, nFailureCount = 0;
		for (uint16_t i = 0; i < GetNode().GetChildCount(); ++i)
		{
			m_Behavior.Setup(GetNode().GetChild(i));
			if (!m_Behavior.IsTerminated())
			{
				m_Behavior.Tick();
			}

			if (m_Behavior.GetStatus() == BH_SUCCESS)
			{
				// �ɹ�һ��
				++nSuccessCount;
				if (m_SuccessPolicy == RequireOne)
				{
					// ֻ��Ҫһ�γɹ��򷵻سɹ�
					return BH_SUCCESS;
				}
			}

			if (m_Behavior.GetStatus() == BH_FAILURE)
			{
				// ʧ��һ��
				++nFailureCount;
				if (m_FailruePolicy == RequireOne)
				{
					// ֻ��Ҫʧ��һ���򷵻�ʧ��
					return BH_FAILURE;
				}
			}
			m_Behavior.Rest();
		}

		if (m_FailruePolicy == RequireAll && nFailureCount == GetNode().GetChildCount())
		{
			// ȫ��ʧ��
			return BH_FAILURE;
		}

		if (m_SuccessPolicy == RequireAll && nSuccessCount == GetNode().GetChildCount())
		{
			// ȫ���ɹ�
			return BH_SUCCESS;
		}

		// ���нڵ㴦��Running
		return BH_RUNNING;
	}

	virtual void OnTerminate(eStatus)
	{
		for (uint16_t i = 0; i < GetNode().GetChildCount(); ++i)
		{
			m_Behavior.Setup(GetNode().GetChild(i));
			if (m_Behavior.IsRunning())
			{
				m_Behavior.Abort();
			}
		}
	}

protected:
	ePolicy m_SuccessPolicy;
	ePolicy m_FailruePolicy;
	CBehavior m_Behavior;
};

typedef CMockComposite<CParallel> CMockParallel;

void testparallel()
{
	CBehaviorTree bt;
	CBehaviorAllocate t;
	CMockParallel &p = t.allocate<CMockParallel>();
	p.Initialize(bt, t, 2);
	CBehavior b(p);
	b.Get<CParallel>()->SetPolicy(CParallel::RequireAll, CParallel::RequireOne);
	bt.Start(b);
	bt.Tick();
}

// �����
// Ϊ���нڵ���Ӽ��
class CMonitor :public CParallel
{
public:
	CMonitor(CComposite &node) :CParallel(node, CParallel::RequireOne, CParallel::RequireOne) {}

	// ��ӿ�ʼ����
	void AddCondition(CNode &condition)
	{
		GetNode().AddChildFront(condition);
	}

	// ��Ӷ���
	void AddAction(CNode &action)
	{
		GetNode().AddChild(action);
	}
};

typedef CMockComposite<CMonitor> CMockMonitor;

void testmonitor()
{
	CBehaviorTree bt;
	CBehaviorAllocate t;
	CMockMonitor &m = t.allocate<CMockMonitor>();
	m.Initialize(bt, t, 2);
	CBehavior b(m);
	bt.Start(b);
	bt.Tick();
}

// ����ѡ����
class CActiveSelector :public CSelector
{
public:
	CActiveSelector(CComposite &node) :
		CSelector(node)
	{
	}

protected:
	virtual void OnInitialize()
	{
		m_CurrentIndex = GetNode().GetChildCount();
	}

	virtual eStatus Update()
	{
		uint16_t previous = m_CurrentIndex;

		// ÿ�ζ��ӵ�һ���ӽڵ㿪ʼ
		CSelector::OnInitialize();
		eStatus result = CSelector::Update();

		// ���ϸ��ڵ���Ч,���ҵ�ǰ�ڵ㲻�����ϸ��ڵ��ʱ��,�������ϸ��ڵ�
		if (previous != GetNode().GetChildCount() && m_CurrentIndex != previous)
		{
			m_CurrentBehavior.Setup(GetNode().GetChild(previous));
			m_CurrentBehavior.Abort();
		}
		//���ص�ǰ�ڵ�״̬
		return result;
	}
};

typedef CMockComposite<CActiveSelector> CMockActiveSelector;

void testactiveselector()
{
	CBehaviorTree bt;
	CBehaviorAllocate t;
	CMockActiveSelector &a = t.allocate<CMockActiveSelector>();
	a.Initialize(bt, t, 2);
	CBehavior b(a);
	bt.Start(b);
	bt.Tick();
}

int main()
{
	test();
	testrepeat();
	testsequence();
	testselector();
	testparallel();
	testmonitor();
	testactiveselector();
	return 0;
}