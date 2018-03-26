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

// ��Ϊ�ڵ�
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

// ��Ϊ�ڵ㹤����
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

// װ�νڵ㣬����ֻ��һ���ڵ�
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

// �ظ�
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
// ��Ͻڵ㣬ӵ�ж���ӽڵ�
class CComposite :public CNode
{
public:
	void AddChild(CMockNode *child) { m_Children.push_back(child); }
	void RemoveChild(CMockNode *child);
	void ClearChildren();

public:
	Nodes m_Children;
};

// ��Ͻڵ㹤����
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

// ���нڵ�
// ˳������,��һ��ʧ��,�򷵻�
// Ҫ���������гɹ�,�򷵻سɹ�,���򷵻�ʧ��
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
				// ����ɹ��ˣ�����һ�������򷵻�
				return s;
			}

			if (++m_CurrentChild == GetNode().m_Children.end())
			{
				// ���нڵ����гɹ�
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
				// ���ʧ���ˣ�����һ�������򷵻�
				return s;
			}

			if (++m_CurrentChild == GetNode().m_Children.end())
			{
				// ���нڵ�����ʧ��
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
		for (Nodes::iterator iter = GetNode().m_Children.begin(); iter != GetNode().m_Children.end(); ++iter)
		{
			m_Behavior.Setup(**iter);
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

		if (m_FailruePolicy == RequireAll && nFailureCount == GetNode().m_Children.size())
		{
			// ȫ��ʧ��
			return BH_FAILURE;
		}

		if (m_SuccessPolicy == RequireAll && nSuccessCount == GetNode().m_Children.size())
		{
			// ȫ���ɹ�
			return BH_SUCCESS;
		}

		// ���нڵ㴦��Running
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

// �����
// Ϊ���нڵ���Ӽ��
class CMonitor :public CParallel
{
public:
	CMonitor(CComposite &node) :CParallel(node, CParallel::RequireOne, CParallel::RequireOne) {}

	// ��ӿ�ʼ����
	void AddCondition(CNode *condition)
	{
		GetNode().m_Children.insert(GetNode().m_Children.begin(), condition);
	}

	// ��Ӷ���
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

// ����ѡ����
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

		// ÿ�ζ��ӵ�һ���ӽڵ㿪ʼ
		CSelector::OnInitialize();
		eStatus result = CSelector::Update();

		// ���ϸ��ڵ���Ч,���ҵ�ǰ�ڵ㲻�����ϸ��ڵ��ʱ��,�������ϸ��ڵ�
		if (previous != GetNode().m_Children.end() && m_CurrentChild != previous)
		{
			m_CurrentBehavior.Setup(**previous);
			m_CurrentBehavior.Abort();
		}
		//���ص�ǰ�ڵ�״̬
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