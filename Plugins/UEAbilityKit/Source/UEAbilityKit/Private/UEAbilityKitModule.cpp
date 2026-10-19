#include "UEAbilityKitModule.h"

DEFINE_LOG_CATEGORY(LogUAK);

void FUEAbilityKitModule::StartupModule()
{
	UE_LOG(LogUAK, Log, TEXT("UEAbilityKit (UAK) loaded."));
}

void FUEAbilityKitModule::ShutdownModule()
{
	UE_LOG(LogUAK, Log, TEXT("UEAbilityKit (UAK) unloaded."));
}

IMPLEMENT_MODULE(FUEAbilityKitModule, UEAbilityKit)
