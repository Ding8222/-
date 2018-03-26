#include <iostream>
#include <string>
#include <vector>
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
};

// 节点基类
class CNode
{
public:
	virtual CTask *Create() = 0;
	virtual void Destroy(CTask *) = 0;

	virtual ~CNode() {}
};

// 行为基类
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

// Node方法在此执行
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

	// 获取该节点行为
	template <class TASK>
	TASK *Get()const
	{
		return dynamic_cast<TASK *>(m_Task);
	}

protected:
	CTask * m_Task;
	CNode *m_Node;
	eStatus m_Status;
};

// 预创建一片k_MaxBehaviorTreeMemory大小的m_Buffer
// 每次分配的时候,从m_Buffer取出一部分内存
const size_t k_MaxBehaviorTreeMemory = 8192;
class CBehaviorTree
{
public:
	CBehaviorTree() :
		m_Buffer(new uint8_t[k_MaxBehaviorTreeMemory]),
		m_Offset(0)
	{
	}

	~CBehaviorTree()
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

// 行为工厂
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
		cout << "OnInitialize" << endl;
		++m_InitializeCalled;
	}

	virtual void OnTerminate(eStatus s)
	{
		cout << "OnTerminate" << endl;
		++m_TerminateCalled;
		m_TerminateStatus = s;
	}

	virtual eStatus Update()
	{
		cout << "Update" << endl;
		++m_UpdateCalled;
		return m_ReturnStatus;
	}
};

// 节点工厂
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
	CMockNode n;
	CBehavior b;
	b.Setup(n);
	b.Tick();
}

void test1()
{
	CBehaviorTree bt;
	CMockNode &nn = bt.allocate<CMockNode>();
	CBehavior &bb = bt.allocate<CBehavior>();
	bb.Setup(nn);
}

// 装饰节点，有且只有一个节点
class CDecorator :public CNode
{
public:
	CDecorator(CNode *child) :m_Child(child) {}
	CNode & GetChild() { return *m_Child; }

protected:
	CNode * m_Child;
};

// 装饰节点工厂类
template <class TASK>
class CMockDecorator :public CDecorator
{
public:
	CMockDecorator(CNode *child):
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

// 重复
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
	CMockNode &n = bt.allocate<CMockNode>();
	CMockRepeat re(&n);
	CBehavior b(re);
	b.Get<CRepeat>()->SetCount(3);
	b.Tick();
}

const size_t k_MaxChildrenPerComposite = 7;
typedef std::vector<CNode*> Nodes;
// 组合节点，拥有多个子节点
class CComposite :public CNode
{
public:
	CComposite():
		m_ChildCount(0)
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
};

// 组合节点工厂类
template <class TASK>
class CMockComposite :public CComposite
{
public:
	void Initialize(CBehaviorTree &tree, size_t size)
	{
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

// 序列节点
// 顺序运行,有一个失败,则返回
// 要求所有运行成功,则返回成功,否则返回失败
class CSequence :public CTask
{
public:
	CSequence(CComposite &node) :
		CTask(node)
	{
	}

	CComposite &GetNode()
	{
		return *static_cast<CComposite *>(m_Node);
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

			if (s != BH_SUCCESS)
			{
				// 如果成功了，则下一个，否则返回
				return s;
			}

			if (++m_CurrentIndex == GetNode().GetChildCount())
			{
				// 所有节点运行成功
				return BH_SUCCESS;
			}

			m_CurrentBehavior.Setup(GetNode().GetChild(m_CurrentIndex));
		}
	}

	CBehavior m_CurrentBehavior;
	uint16_t m_CurrentIndex;
};

typedef CMockComposite<CSequence> CMockSequence;

void testsequence()
{
	CBehaviorTree t;
	CMockSequence &se = t.allocate<CMockSequence>();
	se.Initialize(t, 1);
	CBehavior b;
	b.Setup(se);
	b.Tick();
}

// 选择阶段
// 顺序运行,有一个成功则直接返回
// 只要有一个运行成功,则返回成功,否则返回失败
class CSelector :public CTask
{
public:
	CSelector(CComposite &node) :
		CTask(node)
	{
	}

protected:
	CComposite &GetNode()
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
				// 如果失败了，则下一个，否则返回
				return s;
			}

			if (++m_CurrentIndex == GetNode().GetChildCount())
			{
				// 所有节点运行失败
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
	CBehaviorTree t;
	CMockSelector &se = t.allocate<CMockSelector>();
	se.Initialize(t, 2);
	CBehavior b(se);
	b.Tick();
}

// 并行节点
// 全部运行,当满足成功或者失败个数条件的时候,返回
// 失败应优先于成功
class CParallel :public CTask
{
public:
	enum ePolicy
	{
		RequireOne,	//单个满足条件
		RequireAll,	//全部满足条件
	};

	CParallel(CComposite &node) :
		CTask(node)
	{
	}


	// 成功类型,失败类型
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
				// 成功一次
				++nSuccessCount;
				if (m_SuccessPolicy == RequireOne)
				{
					// 只需要一次成功则返回成功
					return BH_SUCCESS;
				}
			}

			if (m_Behavior.GetStatus() == BH_FAILURE)
			{
				// 失败一次
				++nFailureCount;
				if (m_FailruePolicy == RequireOne)
				{
					// 只需要失败一次则返回失败
					return BH_FAILURE;
				}
			}
			m_Behavior.Rest();
		}

		if (m_FailruePolicy == RequireAll && nFailureCount == GetNode().GetChildCount())
		{
			// 全部失败
			return BH_FAILURE;
		}

		if (m_SuccessPolicy == RequireAll && nSuccessCount == GetNode().GetChildCount())
		{
			// 全部成功
			return BH_SUCCESS;
		}

		// 还有节点处于Running
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
	CBehaviorTree t;
	CMockParallel &p = t.allocate<CMockParallel>();
	p.Initialize(t, 2);
	CBehavior b(p);
	b.Get<CParallel>()->SetPolicy(CParallel::RequireAll, CParallel::RequireOne);
	b.Tick();
}

// 监控器
// 为并行节点添加监控
class CMonitor :public CParallel
{
public:
	CMonitor(CComposite &node) :CParallel(node, CParallel::RequireOne, CParallel::RequireOne) {}

	// 添加开始条件
	void AddCondition(CNode &condition)
	{
		GetNode().AddChildFront(condition);
	}

	// 添加动作
	void AddAction(CNode &action)
	{
		GetNode().AddChild(action);
	}
};

typedef CMockComposite<CMonitor> CMockMonitor;

void testmonitor()
{
	CBehaviorTree t;
	CMockMonitor &m = t.allocate<CMockMonitor>();
	m.Initialize(t, 2);
	CBehavior b(m);
	b.Tick();
}

// 主动选择器
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

		// 每次都从第一个子节点开始
		CSelector::OnInitialize();
		eStatus result = CSelector::Update();

		// 当上个节点有效,而且当前节点不等于上个节点的时候,结束掉上个节点
		if (previous != GetNode().GetChildCount() && m_CurrentIndex != previous)
		{
			m_CurrentBehavior.Setup(GetNode().GetChild(previous));
			m_CurrentBehavior.Abort();
		}
		//返回当前节点状态
		return result;
	}
};

typedef CMockComposite<CActiveSelector> CMockActiveSelector;

void testactiveselector()
{
	CBehaviorTree t;
	CMockActiveSelector &a = t.allocate<CMockActiveSelector>();
	a.Initialize(t, 2);
	CBehavior b(a);
	b.Tick();
}

int main()
{
	// testrepeat();
	// testsequence();
	// testselector();
	// testparallel();
	// testmonitor();
	testactiveselector();
	return 0;
}