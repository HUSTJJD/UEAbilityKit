#include "UAKTrace.h"

int32 UAKTrace::AddNode(int32 ParentIndex, EAKTraceNodeKind Kind, const FString& Label, float Time)
{
	FAKTraceNode Node;
	Node.ParentIndex = ParentIndex;
	Node.Kind = Kind;
	Node.Label = Label;
	Node.Time = Time;

	const int32 Index = Nodes.Add(MoveTemp(Node));
	if (Index == 0)
	{
		RootIndex = Index;
	}
	return Index;
}

void UAKTrace::AddDetail(int32 NodeIndex, const FString& Detail)
{
	if (Nodes.IsValidIndex(NodeIndex))
	{
		Nodes[NodeIndex].Details.Add(Detail);
	}
}

void UAKTrace::Reset()
{
	Nodes.Reset();
	RootIndex = INDEX_NONE;
}

UAKTrace* UAKTraceSubsystem::BeginTrace(const FString& Label)
{
	if (!bRecordingEnabled)
	{
		return nullptr;
	}

	UAKTrace* Trace = NewObject<UAKTrace>(this);
	Trace->AddNode(INDEX_NONE, EAKTraceNodeKind::Ability, Label, 0.f);
	ActiveTraces.Add(Trace);
	return Trace;
}

void UAKTraceSubsystem::EndTrace(UAKTrace* Trace)
{
	if (!Trace)
	{
		return;
	}

	ActiveTraces.Remove(Trace);
	FinishedTraces.Add(Trace);

	// 只保留最近若干条，避免长时间运行后无限增长
	constexpr int32 MaxFinished = 32;
	while (FinishedTraces.Num() > MaxFinished)
	{
		FinishedTraces.RemoveAt(0);
	}
}
