#include "Prefabs.h"

// nameless namespaces are a way to restrict/control global data
// I prefer them to the singleton design pattern 
namespace 
{
	std::map<std::string, flecs::entity> prefabMap;
}
// functions defined in this file have access to the data in the nameless namespace above
namespace TeamYellow
{
	// interface implementations to access protected data set above
	bool RegisterPrefab(const char* prefabName, const flecs::entity inPrefab)
	{
		auto iter = prefabMap.find(prefabName);
		if (iter == prefabMap.end()) {
			prefabMap[std::string(prefabName)] = inPrefab;
			return true;
		}
		return false; // already exists
	}
	bool RetreivePrefab(const char* prefabName, flecs::entity& outPrefab)
	{
		auto iter = prefabMap.find(prefabName);
		if (iter != prefabMap.end()) {
			outPrefab = prefabMap[std::string(prefabName)];
			return true;
		}
		return false; // prefab not found
	}
	bool UnregisterPrefab(const char* prefabName)
	{
		auto iter = prefabMap.find(prefabName);
		if (iter != prefabMap.end()) {
			prefabMap.erase(iter);
			return true;
		}
		return false; // prefab not found
	}
}
