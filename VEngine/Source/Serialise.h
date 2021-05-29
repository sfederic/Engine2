#pragma once

#include <unordered_map>
#include <typeindex>
#include <optional>
#include <assert.h>
#include <string>
#include <fstream>

class Actor;

//Lot of good references and ideas on serialisation. Heaps of ideas on reflection, custom compile stages, header parsing.
//REF:https://google.github.io/flatbuffers/flatbuffers_guide_tutorial.html
//REF:http://donw.io/post/reflection-cpp-2/

//Container for storing variable data and variable types against a name on registration.
//Used with Actors during serialisation and creating property widgets.
//Properties are created in GetProps() function blocks so that the maps can be cascaded down
//actor inheritence trees, meaning you have to define Properties by hand in that same function, then
//call it when needed. Maybe look into some way to cache it. Performance wasn't a huge issue in testing.
struct Properties
{
	template <typename T>
	void Add(const std::string& name, T* data)
	{
		dataMap[name] = data;
		typeMap[name] = typeid(T);
	}

	void* GetData(const std::string& dataName)
	{
		auto& dataMapIt = dataMap.find(dataName);
		if(dataMapIt != dataMap.end())
		{
			return dataMapIt->second;
		}

		return nullptr; //Want to be able to fail finding keys when serialising.
	}

	std::type_index GetType(const std::string& typeName)
	{
		auto& typeMapIt = typeMap.find(typeName);
		return typeMapIt->second.value(); //Type will usually be grabbed after data, failing here acts as a check.
	}

	std::unordered_map<std::string, void*> dataMap;
	std::unordered_map<std::string, std::optional<std::type_index>> typeMap;
};

//Level save text file structure looks like this:
//ActorSystem Name
//Num of actors to spawn
//Actor Property - eg. Position
//Property value - eg. 1 -4 69.5
//next

struct Serialiser
{
	Serialiser(const std::string& file, std::ios_base::openmode mode);

	//TODO: I don't like these being static here.
	static void Serialise(Actor* actor, std::ostream& os);
	static void Deserialise(Actor* actor, std::istream& is);

	std::filebuf fb;
};
