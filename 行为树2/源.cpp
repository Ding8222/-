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

class CNode
{
public:
	virtual CTask *Create() = 0;
	virtual void Destroy(CTask *) = 0;

	virtual ~CNode() {}
};

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

// 行为节点
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

// 行为节点工厂类
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
	CMockTask* t = b.Get<CMockTask>();
	b.Tick();
}

// 装饰节点，有且只有一个节点
class CDecorator :public CNode
{
protected:
	CNode * m_pChild;
public:
	CNode &GetChild() { return *m_pChild; }
	CDecorator(CNode *child) :m_pChild(child) {}
};

template <class TASK>
class CMockDecorator :public CDecorator
{
public:
	CMockDecorator(CNode *child) :
		CDecorator(child)
	{

	}

	~CMockDecorator()
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
	CRepeat(CDecorator &child) :CTask(child) {}

	CDecorator& getNode()
	{
		return *static_cast<CDecorator*>(m_Node);
	}

	void SetCount(int count)
	{
		m_Limit = count;
	}

	void OnInitialize()
	{
		m_Counter = 0;
		m_CurrentBehavior.Setup(getNode().GetChild());
	}

	virtual eStatus Update()
	{
		for (;;)
		{
			m_CurrentBehavior.Tick();
			if (m_CurrentBehavior.GetStatus() == BH_RUNNING) break;
			if (m_CurrentBehavior.GetStatus() == BH_FAILURE) return BH_FAILURE;
			if (++m_Counter >= m_Limit) return BH_SUCCESS;
			m_CurrentBehavior.Rest();
		}
		return BH_INVALID;
	}
	
protected:
	int m_Limit;
	int m_Counter;
	CBehavior m_CurrentBehavior;
};

typedef CMockDecorator<CRepeat> CMockRepeat;

void testrepeat()
{
	CMockNode n;
	CMockRepeat r(&n);
	CBehavior b(r);
	b.Get<CRepeat>()->SetCount(10);
	b.Tick();
}

typedef std::vector<CNode*> Nodes;
// 组合节点，拥有多个子节点
class CComposite :public CNode
{
public:
	void AddChild(CMockNode *child) { m_Children.push_back(child); }
	void RemoveChild(CMockNode *child);
	void ClearChildren();

public:
	Nodes m_Children;
};

// 组合节点工厂类
template <class TASK>
class CMockComposite :public CComposite
{
public:
	CMockComposite(size_t size)
	{
		for (size_t i = 0; i < size; ++i)
		{
			m_Children.push_back(new CMockNode);
		}
	}

	~CMockComposite()
	{
		for (size_t i = 0; i < m_Children.size(); ++i)
		{
			delete m_Children[i];
		}
	}

	CMockTask &operator[](size_t index)
	{
		assert(index < m_Children.size());
		CMockTask *task = static_cast<CMockNode *>(m_Children[index])->m_Task;
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
		m_CurrentChild = GetNode().m_Children.begin();
		m_CurrentBehavior.Setup(**m_CurrentChild);
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

			if (++m_CurrentChild == GetNode().m_Children.end())
			{
				// 所有节点运行成功
				return BH_SUCCESS;
			}

			m_CurrentBehavior.Setup(**m_CurrentChild);
		}
	}

protected:
	Nodes::iterator m_CurrentChild;
	CBehavior m_CurrentBehavior;
};

typedef CMockComposite<CSequence> CMockSequence;

void testsequence()
{
	CMockSequence s(2);
	CBehavior b(s);
	b.Tick();
	s[0].m_ReturnStatus = BH_SUCCESS;
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
		m_CurrentChild = GetNode().m_Children.begin();
		m_CurrentBehavior.Setup(**m_CurrentChild);
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

			if (++m_CurrentChild == GetNode().m_Children.end())
			{
				// 所有节点运行失败
				return BH_FAILURE;
			}

			m_CurrentBehavior.Setup(**m_CurrentChild);
		}
	}

protected:
	Nodes::iterator m_CurrentChild;
	CBehavior m_CurrentBehavior;
};

typedef CMockComposite<CSelector> CMockSelector;

void testselector()
{
	CMockSelector s(2);
	CBehavior b(s);
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
		for (Nodes::iterator iter = GetNode().m_Children.begin(); iter != GetNode().m_Children.end(); ++iter)
		{
			m_Behavior.Setup(**iter);
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

		if (m_FailruePolicy == RequireAll && nFailureCount == GetNode().m_Children.size())
		{
			// 全部失败
			return BH_FAILURE;
		}

		if (m_SuccessPolicy == RequireAll && nSuccessCount == GetNode().m_Children.size())
		{
			// 全部成功
			return BH_SUCCESS;
		}

		// 还有节点处于Running
		return BH_RUNNING;
	}

	virtual void OnTerminate(eStatus)
	{
		for (Nodes::iterator iter = GetNode().m_Children.begin(); iter != GetNode().m_Children.end(); ++iter)
		{
			m_Behavior.Setup(**iter);
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
 	CMockParallel p(2);
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
	void AddCondition(CNode *condition)
	{
		GetNode().m_Children.insert(GetNode().m_Children.begin(), condition);
	}

	// 添加动作
	void AddAction(CNode *action)
	{
		GetNode().m_Children.push_back(action);
	}
};

typedef CMockComposite<CMonitor> CMockMonitor;

void testmonitor()
{
	CMockMonitor m(2);
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

	virtual void OnInitialize()
	{
		m_CurrentChild = GetNode().m_Children.end();
	}

	virtual eStatus Update()
	{
		Nodes::iterator previous = m_CurrentChild;

		// 每次都从第一个子节点开始
		CSelector::OnInitialize();
		eStatus result = CSelector::Update();

		// 当上个节点有效,而且当前节点不等于上个节点的时候,结束掉上个节点
		if (previous != GetNode().m_Children.end() && m_CurrentChild != previous)
		{
			m_CurrentBehavior.Setup(**previous);
			m_CurrentBehavior.Abort();
		}
		//返回当前节点状态
		return result;
	}
};

typedef CMockComposite<CActiveSelector> CMockActiveSelector;

void testactiveselector()
{
	CMockActiveSelector a(2);
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
	//testactiveselector();
	return 0;
}