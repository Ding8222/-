#include <iostream>
#include <string>
#include <vector>

using namespace std;

enum eStatus
{
	BH_INVALID,
	BH_SUCCESS,
	BH_FAILURE,
	BH_RUNNING,
	BH_ABORTED,
};

// 行为节点
class CBehavior
{
public:
	virtual eStatus Update() = 0;
	virtual void OnInitialize() {}
	virtual void OnTerminate(eStatus) {}

	CBehavior() : m_Status(BH_INVALID) {}
	virtual ~CBehavior() {}

	eStatus Tick()
	{
		if (m_Status != BH_RUNNING)
		{
			OnInitialize();
		}

		m_Status = Update();

		if (m_Status != BH_RUNNING)
		{
			OnTerminate(m_Status);
		}

		return m_Status;
	}

	void Rest()
	{
		m_Status = BH_INVALID;
	}

	void Abort()
	{
		OnTerminate(BH_ABORTED);
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

private:
	eStatus m_Status;
};

// 行为节点工厂类
struct CMockBehavior :public CBehavior
{
	int m_InitializeCalled;
	int m_TerminateCalled;
	int m_UpdateCalled;
	eStatus m_ReturnStatus;
	eStatus m_TerminateStatus;

	CMockBehavior() :m_InitializeCalled(0),
		m_TerminateCalled(0),
		m_UpdateCalled(0),
		m_ReturnStatus(BH_RUNNING),
		m_TerminateStatus(BH_INVALID)
	{
	}

	virtual ~CMockBehavior()
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

void test1()
{
	CMockBehavior b;
	b.Tick();
	b.m_ReturnStatus = BH_SUCCESS;
	b.Tick();
}

// 装饰节点，有且只有一个节点
class CDecorator :public CBehavior
{
protected:
	CBehavior * m_pChild;
public:
	CDecorator(CBehavior *child) :m_pChild(child) {}
};

template <class DECORATOR>
class CMockDecorator :public DECORATOR
{
public:
	CMockDecorator(CBehavior *child):
		DECORATOR(child)
	{

	}

	~CMockDecorator()
	{

	}
};

// 重复
class CRepeat :public CDecorator
{
public:
	CRepeat(CBehavior *child) :CDecorator(child) {}

	void SetCount(int count)
	{
		m_Limit = count;
	}

	void OnInitialize()
	{
		m_Counter = 0;
	}

	eStatus Update()
	{
		for (;;)
		{
			m_pChild->Tick();
			if (m_pChild->GetStatus() == BH_RUNNING) break;
			if (m_pChild->GetStatus() == BH_FAILURE) return BH_FAILURE;
			if (++m_Counter >= m_Limit) return BH_SUCCESS;
			m_pChild->Rest();
		}
		return BH_INVALID;
	}

protected:
	int m_Limit;
	int m_Counter;
};

typedef CMockDecorator<CRepeat> CMockRepeat;

void test2()
{
	CMockBehavior b;
	b.m_ReturnStatus = BH_SUCCESS;
	CRepeat re(&b);
	re.SetCount(3);
	re.Tick();
}

void test3()
{
	CMockBehavior b;
	b.m_ReturnStatus = BH_SUCCESS;
	CMockRepeat re(&b);
	re.SetCount(3);
	re.Tick();
}

// 组合节点，拥有多个子节点
class CCompsite :public CBehavior
{
public:
	void AddChild(CBehavior *child) { m_Children.push_back(child); }
	void RemoveChild(CBehavior *child);
	void ClearChildren();

protected:
	typedef std::vector<CBehavior*> Behaviors;
	Behaviors m_Children;
};

// 组合节点工厂类
template <class COMPOSITE>
class CMockComposite :public COMPOSITE
{
public:
	CMockComposite(size_t size)
	{
		for (size_t i = 0; i < size; ++i)
		{
			COMPOSITE::m_Children.push_back(new CMockBehavior);
		}
	}

	~CMockComposite()
	{
		for (size_t i = 0; i < COMPOSITE::m_Children.size(); ++i)
		{
			delete COMPOSITE::m_Children[i];
		}
	}

	CMockBehavior &operator[](size_t index)
	{
		assert(index < COMPOSITE::m_Children.size());
		return *static_cast<CMockBehavior*>(COMPOSITE::m_Children[index]);
	}
};

// 序列节点
// 顺序运行,有一个失败,则返回
// 要求所有运行成功,则返回成功,否则返回失败
class CSequence :public CCompsite
{
protected:
	virtual ~CSequence() {}

	virtual void OnInitialize()
	{
		m_CurrentChild = m_Children.begin();
	}

	virtual eStatus Update()
	{
		for (;;)
		{
			eStatus s = (*m_CurrentChild)->Tick();

			if (s != BH_SUCCESS)
			{
				// 如果成功了，则下一个，否则返回
				return s;
			}

			if (++m_CurrentChild == m_Children.end())
			{
				// 所有节点运行成功
				return BH_SUCCESS;
			}
		}
	}

	Behaviors::iterator m_CurrentChild;
};

typedef CMockComposite<CSequence> CMockSequence;

void test4()
{
	CMockSequence s(2);
	s.Tick();
}

// 选择阶段
// 顺序运行,有一个成功则直接返回
// 只要有一个运行成功,则返回成功,否则返回失败
class CSelector :public CCompsite
{
protected:
	virtual ~CSelector() {}

	virtual void OnInitialize()
	{
		m_Current = m_Children.begin();
	}

	virtual eStatus Update()
	{
		for (;;)
		{
			eStatus s = (*m_Current)->Tick();

			if (s != BH_FAILURE)
			{
				// 如果失败了，则下一个，否则返回
				return s;
			}

			if (++m_Current == m_Children.end())
			{
				// 所有节点运行失败
				return BH_FAILURE;
			}
		}
	}

	Behaviors::iterator m_Current;
};

typedef CMockComposite<CSelector> CMockSelector;

void test5()
{
	CMockSelector s(2);
}

// 并行节点
// 全部运行,当满足成功或者失败个数条件的时候,返回
// 失败应优先于成功
class CParallel :public CCompsite
{
public:
	enum ePolicy
	{
		RequireOne,	//单个满足条件
		RequireAll,	//全部满足条件
	};

	// 成功类型,失败类型
	CParallel(ePolicy forSuccess, ePolicy forFailure) :
		m_SuccessPolicy(forSuccess),
		m_FailruePolicy(forFailure)
	{

	}

	virtual ~CParallel() {}

protected:
	virtual eStatus Update()
	{
		size_t nSuccessCount = 0, nFailureCount = 0;
		for (Behaviors::iterator iter = m_Children.begin(); iter != m_Children.end(); ++iter)
		{
			CBehavior &b = **iter;
			if (!b.IsTerminated())
			{
				b.Tick();
			}

			if (b.GetStatus() == BH_SUCCESS)
			{
				// 成功一次
				++nSuccessCount;
				if (m_SuccessPolicy == RequireOne)
				{
					// 只需要一次成功则返回成功
					return BH_SUCCESS;
				}
			}

			if (b.GetStatus() == BH_FAILURE)
			{
				// 失败一次
				++nFailureCount;
				if (m_FailruePolicy == RequireOne)
				{
					// 只需要失败一次则返回失败
					return BH_FAILURE;
				}
			}
		}

		if (m_FailruePolicy == RequireAll && nFailureCount == m_Children.size())
		{
			// 全部失败
			return BH_FAILURE;
		}

		if (m_SuccessPolicy == RequireAll && nSuccessCount == m_Children.size())
		{
			// 全部成功
			return BH_SUCCESS;
		}

		// 还有节点处于Running
		return BH_RUNNING;
	}

	virtual void OnTerminate(eStatus)
	{
		for (Behaviors::iterator iter = m_Children.begin(); iter != m_Children.end(); ++iter)
		{
			CBehavior &b = **iter;
			if (b.IsRunning())
			{
				b.Abort();
			}
		}
	}

protected:
	ePolicy m_SuccessPolicy;
	ePolicy m_FailruePolicy;
};

void test6()
{
	CParallel p(CParallel::RequireAll, CParallel::RequireOne);
	CMockBehavior children[2];
	p.AddChild(&children[0]);
	p.AddChild(&children[1]);
}

// 监控器
// 为并行节点添加监控
class CMonitor :public CParallel
{
public:
	CMonitor() :CParallel(CParallel::RequireOne, CParallel::RequireOne) {}

	// 添加开始条件
	void AddCondition(CBehavior *condition)
	{
		m_Children.insert(m_Children.begin(), condition);
	}

	// 添加结束动作
	void AddAction(CBehavior *action)
	{
		m_Children.push_back(action);
	}
};

typedef CMockComposite<CMonitor> CMockMonitor;

void test7()
{
	CMockMonitor m(2);
	m.Tick();
}

// 主动选择器
class CActiveSelector :public CSelector
{
protected:
	virtual void OnInitialize()
	{
		m_Current = m_Children.end();
	}

	virtual eStatus Update()
	{
		Behaviors::iterator previous = m_Current;

		// 每次都从第一个子节点开始
		CSelector::OnInitialize();
		eStatus result = CSelector::Update();

		// 当上个节点有效,而且当前节点不等于上个节点的时候,结束掉上个节点
		if (previous != m_Children.end() && m_Current != previous)
		{
			(*previous)->OnTerminate(BH_ABORTED);
		}
		//返回当前节点状态
		return result;
	}
};

typedef CMockComposite<CActiveSelector> CMockActiveSelector;

void test8()
{
	CMockActiveSelector a(2);

	a.Tick();
}

int main()
{
	test1();
	test2();
	test3();
	test4();
	test5();
	test6();
	test7();
	test8();
	return 0;
}